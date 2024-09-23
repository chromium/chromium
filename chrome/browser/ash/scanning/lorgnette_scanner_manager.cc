// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "chrome/browser/ash/scanning/lorgnette_notification_controller.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_util.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "chromeos/ash/components/scanning/scanner.h"
#include "components/device_event_log/device_event_log.h"
#include "net/base/ip_address.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {

namespace {

// Used as the client ID when calling ListScanners to retrieve scanner names.
constexpr char kListScannersDiscoveryClientId[] = "GetScannerNames";
// Used as the client ID when verifying scanner connectivity.
constexpr char kVerifyScannerClientId[] = "ZeroconfScannerChecker";

// A list of Epson models that do not rotate alternating ADF scanned pages
// to be excluded in IsRotateAlternate().
constexpr char kEpsonNoFlipModels[] =
    "\\b("
    "AM-C400"
    "|AM-C4000"
    "|AM-C5000"
    "|AM-C550"
    "|AM-C6000"
    "|DS-790WN"
    "|DS-800WN"
    "|DS-900WN"
    "|DS-C420W"
    "|DS-C480W"
    "|EM-C800"
    "|ES-C320W"
    "|ES-C380W"
    "|LM-C400"
    "|LM-C4000"
    "|LM-C5000"
    "|LM-C6000"
    "|LP-M8180A"
    "|LP-M8180F"
    "|LX-10020M"
    "|LX-10050KF"
    "|LX-10050MF"
    "|LX-6050MF"
    "|LX-7550MF"
    "|PX-M382F"
    "|PX-M7070FX"
    "|PX-M7080FX"
    "|PX-M7090FX"
    "|PX-M7110F"
    "|PX-M7110FP"
    "|PX-M860F"
    "|PX-M880FX"
    "|PX-M890FX"
    "|RR-400W"
    "|WF-6530"
    "|WF-6590"
    "|WF-6593"
    "|WF-C20600"
    "|WF-C20600a"
    "|WF-C20600c"
    "|WF-C20750"
    "|WF-C20750a"
    "|WF-C20750c"
    "|WF-C21000"
    "|WF-C21000a"
    "|WF-C21000c"
    "|WF-C579R"
    "|WF-C579Ra"
    "|WF-C8610"
    "|WF-C8690"
    "|WF-C8690a"
    "|WF-C869R"
    "|WF-C869Ra"
    "|WF-C878R"
    "|WF-C878Ra"
    "|WF-C879R"
    "|WF-C879Ra"
    "|WF-M5899"
    "|WF-M21000"
    "|WF-M21000a"
    "|WF-M21000c"
    ")\\b";

// A prioritized list of scan protocols. Protocols that appear earlier in the
// list are preferred over those that appear later in the list when
// communicating with a connected scanner.
constexpr std::array<ScanProtocol, 4> kPrioritizedProtocols = {
    ScanProtocol::kEscls, ScanProtocol::kEscl, ScanProtocol::kLegacyNetwork,
    ScanProtocol::kLegacyUsb};

// Returns a pointer to LorgnetteManagerClient, which is used to detect and
// interact with scanners via the lorgnette D-Bus service.
LorgnetteManagerClient* GetLorgnetteManagerClient() {
  return LorgnetteManagerClient::Get();
}

// Creates a base name by concatenating the manufacturer and model, if the
// model doesn't already include the manufacturer. Appends "(USB)" for USB
// scanners.
std::string CreateBaseName(const lorgnette::ScannerInfo& lorgnette_scanner,
                           const bool is_usb_scanner) {
  const std::string model = lorgnette_scanner.model();
  const std::string manufacturer = lorgnette_scanner.manufacturer();

  // It's assumed that, if present, the manufacturer would be the first word in
  // the model.
  const std::string maybe_manufacturer =
      RE2::PartialMatch(model.c_str(), base::StringPrintf("(?i)\\A%s\\b",
                                                          manufacturer.c_str()))
          ? ""
          : manufacturer + " ";

  return base::StringPrintf("%s%s%s", maybe_manufacturer.c_str(), model.c_str(),
                            is_usb_scanner ? " (USB)" : "");
}

std::string ScannerCapabilitiesToString(
    const lorgnette::ScannerCapabilities& capabilities) {
  std::vector<std::string> sources;
  sources.reserve(capabilities.sources_size());
  for (const lorgnette::DocumentSource& source : capabilities.sources()) {
    std::vector<std::string> resolutions;
    resolutions.reserve(source.resolutions_size());
    for (const uint32_t resolution : source.resolutions()) {
      resolutions.emplace_back(base::StringPrintf("%d", resolution));
    }

    std::vector<std::string> color_modes;
    color_modes.reserve(source.color_modes_size());
    for (int i = 0; i < source.color_modes_size(); i++) {
      // Loop manually because `color_modes()` returns a RepeatedField<int>
      // instead of ColorMode.
      color_modes.emplace_back(
          lorgnette::ColorMode_Name(source.color_modes(i)));
    }

    sources.emplace_back(base::StringPrintf(
        "{ %s (%s) area=%0.1fx%0.1f resolutions=%s color_modes=%s }",
        lorgnette::SourceType_Name(source.type()).c_str(),
        source.name().c_str(), source.area().width(), source.area().height(),
        base::JoinString(resolutions, ",").c_str(),
        base::JoinString(color_modes, ",").c_str()));
  }
  return base::JoinString(sources, ", ");
}

// Create a unique ID for a scanner based off the scanner's UUID and its
// connection string.  A single scanner can have multiple ways to connect to it
// (http and https, for example), and the UUID will be the same between these
// two connection strings (since the UUID should identify a unique device and
// not the connection protocol).  So, UUID itself is not unique.  UUID combined
// with the connection string should be unique.
std::string CreateScannerId(std::string_view uuid,
                            std::string_view connection_string) {
  return base::StrCat({uuid, ":", connection_string});
}

class LorgnetteScannerManagerImpl final : public LorgnetteScannerManager {
 public:
  LorgnetteScannerManagerImpl(
      std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector,
      Profile* profile)
      : zeroconf_scanner_detector_(std::move(zeroconf_scanner_detector)) {
    zeroconf_scanner_detector_->RegisterScannersDetectedCallback(
        base::BindRepeating(&LorgnetteScannerManagerImpl::OnScannersDetected,
                            weak_ptr_factory_.GetWeakPtr()));
    OnScannersDetected(zeroconf_scanner_detector_->GetScanners());
    lorgnette_notification_controller_ =
        std::make_unique<LorgnetteNotificationController>(profile);
  }

  ~LorgnetteScannerManagerImpl() override = default;

  // KeyedService:
  void Shutdown() override { weak_ptr_factory_.InvalidateWeakPtrs(); }

  // LorgnetteScannerManager:
  void GetScannerNames(GetScannerNamesCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    GetLorgnetteManagerClient()->ListScanners(
        kListScannersDiscoveryClientId,
        /*local_only=*/false,
        /*preferred_only=*/true,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnListScannerNamesResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void GetScannerInfoList(const std::string& client_id,
                          LocalScannerFilter local_only,
                          SecureScannerFilter secure_only,
                          GetScannerInfoListCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    GetLorgnetteManagerClient()->ListScanners(
        client_id, (local_only == LocalScannerFilter::kLocalScannersOnly),
        /*preferred_only=*/false,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnListScannerInfoResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(client_id),
                       std::move(callback), local_only, secure_only));
  }

  // LorgnetteScannerManager:
  void GetScannerCapabilities(
      const std::string& scanner_name,
      GetScannerCapabilitiesCallback callback) override {
    std::string device_name;
    ScanProtocol protocol;
    if (!GetUsableDeviceNameAndProtocol(scanner_name, device_name, protocol)) {
      PRINTER_LOG(ERROR) << "GetScannerCapabilities failed for: "
                         << scanner_name;
      std::move(callback).Run(std::nullopt);
      return;
    }

    GetLorgnetteManagerClient()->GetScannerCapabilities(
        device_name,
        base::BindOnce(
            &LorgnetteScannerManagerImpl::OnScannerCapabilitiesResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback), scanner_name,
            device_name, protocol));
  }

  // LorgnetteScannerManager:
  void OpenScanner(const lorgnette::OpenScannerRequest& request,
                   OpenScannerCallback callback) override {
    std::string connection_string = request.scanner_id().connection_string();

    // If the client doesn't have any tokens, whatever they supplied can't be
    // valid.
    TokenToScannerId* valid_tokens =
        base::FindOrNull(client_tokens_, request.client_id());
    if (!valid_tokens) {
      lorgnette::OpenScannerResponse response;
      *response.mutable_scanner_id() = request.scanner_id();
      response.set_result(lorgnette::OPERATION_RESULT_INVALID);
      PRINTER_LOG(ERROR) << "OpenScanner: No valid tokens for "
                         << connection_string;
      std::move(callback).Run(std::move(response));
      return;
    }

    // If the token isn't found in the previously returned set, it isn't valid.
    std::optional<ScannerId>* device_id =
        base::FindOrNull(*valid_tokens, connection_string);
    if (!device_id) {
      lorgnette::OpenScannerResponse response;
      *response.mutable_scanner_id() = request.scanner_id();
      response.set_result(lorgnette::OPERATION_RESULT_INVALID);
      PRINTER_LOG(ERROR) << "OpenScanner: No device ID for "
                         << connection_string;
      std::move(callback).Run(std::move(response));
      return;
    }

    // If the token is found but doesn't have a value, the referenced device is
    // no longer available.
    if (!device_id->has_value()) {
      lorgnette::OpenScannerResponse response;
      *response.mutable_scanner_id() = request.scanner_id();
      response.set_result(lorgnette::OPERATION_RESULT_MISSING);
      PRINTER_LOG(ERROR) << "OpenScanner: Empty device ID for "
                         << connection_string;
      std::move(callback).Run(std::move(response));
      return;
    }

    // Token is valid.  The necessary SANE connection string is the second
    // field.
    connection_string = device_id->value().second;
    lorgnette::OpenScannerRequest lorgnette_request = request;
    lorgnette_request.mutable_scanner_id()->set_connection_string(
        connection_string);
    PRINTER_LOG(EVENT) << "OpenScanner for " << connection_string;
    GetLorgnetteManagerClient()->OpenScanner(
        std::move(lorgnette_request),
        base::BindOnce(&LorgnetteScannerManagerImpl::OnOpenScannerResponse,
                       weak_ptr_factory_.GetWeakPtr(), request.scanner_id(),
                       std::move(callback)));
  }

  // LorgnetteScannerManager:
  void CloseScanner(const lorgnette::CloseScannerRequest& request,
                    CloseScannerCallback callback) override {
    PRINTER_LOG(EVENT) << "CloseScanner: " << request.scanner().token();
    GetLorgnetteManagerClient()->CloseScanner(
        request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnCloseScannerResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void SetOptions(const lorgnette::SetOptionsRequest& request,
                  SetOptionsCallback callback) override {
    GetLorgnetteManagerClient()->SetOptions(
        request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnSetOptionsResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void GetCurrentConfig(const lorgnette::GetCurrentConfigRequest& request,
                        GetCurrentConfigCallback callback) override {
    GetLorgnetteManagerClient()->GetCurrentConfig(
        request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnGetCurrentConfigResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void StartPreparedScan(const lorgnette::StartPreparedScanRequest& request,
                         StartPreparedScanCallback callback) override {
    GetLorgnetteManagerClient()->StartPreparedScan(
        request, base::BindOnce(
                     &LorgnetteScannerManagerImpl::OnStartPreparedScanResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  void ReadScanData(const lorgnette::ReadScanDataRequest& request,
                    ReadScanDataCallback callback) override {
    GetLorgnetteManagerClient()->ReadScanData(
        request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnReadScanDataResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // LorgnetteScannerManager:
  bool IsRotateAlternate(const std::string& scanner_name,
                         const std::string& source_name) override {
    if (!RE2::PartialMatch(source_name, RE2("(?i)adf duplex"))) {
      return false;
    }

    std::string device_name;
    ScanProtocol protocol;
    if (!GetUsableDeviceNameAndProtocol(scanner_name, device_name, protocol)) {
      PRINTER_LOG(ERROR) << "IsRotateAlternate: Failed to get device name for "
                         << scanner_name;
      return false;
    }

    std::string exclude_regex = std::string("^(airscan|ippusb).*(EPSON\\s+)?") +
                                std::string(kEpsonNoFlipModels);
    if (RE2::PartialMatch(device_name, RE2("^(epsonds|epson2)")) ||
        RE2::PartialMatch(device_name, RE2(exclude_regex))) {
      return false;
    }

    return RE2::PartialMatch(device_name, RE2("(?i)epson"));
  }

  // LorgnetteScannerManager:
  void Scan(const std::string& scanner_name,
            const lorgnette::ScanSettings& settings,
            ProgressCallback progress_callback,
            PageCallback page_callback,
            CompletionCallback completion_callback) override {
    std::string device_name;
    ScanProtocol protocol;  // Unused.
    if (!GetUsableDeviceNameAndProtocol(scanner_name, device_name, protocol)) {
      std::move(completion_callback).Run(lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
      return;
    }

    GetLorgnetteManagerClient()->StartScan(
        device_name, settings, std::move(completion_callback),
        std::move(page_callback), std::move(progress_callback));
  }

  // LorgnetteScannerManager:
  void CancelScan(CancelCallback cancel_callback) override {
    GetLorgnetteManagerClient()->CancelScan(std::move(cancel_callback));
  }

  // LorgnetteScannerManager:
  void CancelScan(const lorgnette::CancelScanRequest& request,
                  CancelScanCallback callback) override {
    GetLorgnetteManagerClient()->CancelScan(
        request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnCancelScanResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Scanner device UUID and connection string, because connection string alone
  // can point to different devices over time.
  using ScannerId = std::pair<std::string, std::string>;
  using TokenToScannerId = std::map<std::string, std::optional<ScannerId>>;

  // Called when scanners are detected by a ScannerDetector.
  void OnScannersDetected(std::vector<Scanner> scanners) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    zeroconf_scanners_ = scanners;
  }

  void SendFinalScannerList(GetScannerNamesCallback callback) {
    std::vector<std::string> scanner_names;
    scanner_names.reserve(deduped_scanners_.size());
    for (const auto& entry : deduped_scanners_)
      scanner_names.push_back(entry.first);

    std::move(callback).Run(std::move(scanner_names));
  }

  // Removes a scanner name from deduped_scanners_ if it has no capabilities. If
  // there are remaining scanners to filter, start the recursive loop again with
  // a call to GetScannerCapabilities with the next scanner in
  // scanners_to_filter_.
  void RemoveScannersIfUnusable(
      GetScannerNamesCallback callback,
      const std::string& scanner_name,
      const std::optional<lorgnette::ScannerCapabilities>& capabilities) {
    if (!capabilities)
      deduped_scanners_.erase(scanner_name);
    scanners_to_filter_.pop_back();
    if (scanners_to_filter_.empty()) {
      SendFinalScannerList(std::move(callback));
    } else {
      GetScannerCapabilities(
          scanners_to_filter_.back(),
          base::BindOnce(&LorgnetteScannerManagerImpl::RemoveScannersIfUnusable,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         scanners_to_filter_.back()));
    }
  }

  // Starts a recursive loop of GetScannerCapabilities,
  // OnScannerCapabilitiesResponse, and RemoveScannersIfUnusable.
  // GetScannerCapabilities takes a scanner name, then creates a loop with
  // OnScannerCapabilitiesResponse to check all usable device names for
  // capabilities, marking them unusable along the way if they return no
  // capabilities. Once all device names have been checked, or capabilities have
  // been found, RemoveScannersIfUnusable is called with the scanner name.
  void FilterScannersAndRespond(GetScannerNamesCallback callback) {
    if (!scanners_to_filter_.empty()) {
      // Run GetScannerCapabilities with a callback that removes scanners from
      // the deduped_scanners_ mapping if none of their names return
      // capabilities.
      GetScannerCapabilities(
          scanners_to_filter_.back(),
          base::BindOnce(&LorgnetteScannerManagerImpl::RemoveScannersIfUnusable,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         scanners_to_filter_.back()));
    } else {
      SendFinalScannerList(std::move(callback));
    }
  }

  // Handles the result of calling LorgnetteManagerClient::ListScanners() for
  // GetScannerNames.
  void OnListScannerNamesResponse(
      GetScannerNamesCallback callback,
      std::optional<lorgnette::ListScannersResponse> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    RebuildDedupedScanners(response);
    FilterScannersAndRespond(std::move(callback));
  }

  // `scanners_to_verify` is a (potentially empty) list of scanners that need to
  // be verified before being passed back to the caller.  If there are some
  // scanners in this list, this will send an `OpenScanner` request to see if
  // the scanner is responsive.  The callback to that request will update the
  // list of scanners that still need to be verified as well as update
  // `response`, and then call this method again to verify the remaining
  // scanners.  When the list is empty, this will simply call `callback` with
  // `response`.
  void VerifyScanners(const std::string& client_id,
                      std::vector<lorgnette::ScannerInfo> scanners_to_verify,
                      lorgnette::ListScannersResponse response,
                      GetScannerInfoListCallback callback) {
    if (scanners_to_verify.empty()) {
      // TODO(nmuggli): Figure out how to associate a lorgnette scanner to a
      // zeroconf scanner.  If they represent the same physical scanner, the
      // ScannerInfo objects should have the same device_uuid.  For now, just
      // ensure each ScannerInfo has a device_uuid (the lorgnette backend is not
      // yet populating the device_uuid).
      for (lorgnette::ScannerInfo& info : *response.mutable_scanners()) {
        if (info.device_uuid().empty()) {
          info.set_device_uuid(
              base::Uuid::GenerateRandomV4().AsLowercaseString());
        }
      }

      UpdateScannerTokens(std::move(client_id), std::move(callback),
                          std::move(response));
      return;
    }

    lorgnette::OpenScannerRequest open_request;
    open_request.mutable_scanner_id()->set_connection_string(
        scanners_to_verify.back().name());
    // Just use a hard-coded client ID for this.  The scanner, if successfully
    // opened, will get closed right away anyhow.
    open_request.set_client_id(kVerifyScannerClientId);
    GetLorgnetteManagerClient()->OpenScanner(
        open_request,
        base::BindOnce(&LorgnetteScannerManagerImpl::OnVerifyScanner,
                       weak_ptr_factory_.GetWeakPtr(), std::move(client_id),
                       std::move(scanners_to_verify), std::move(response),
                       std::move(callback)));
  }

  // Called in response to an `OpenScanner` request while checking if a scanner
  // is responsive.  This works in conjunction with `VerifyScanners` and will
  // update the list of scanners still left to verify.  This will populate
  // `list_response` based on `open_response` and will call `VerifyScanners`
  // once that has happened to verify any remaining scanners.
  void OnVerifyScanner(
      const std::string client_id,
      std::vector<lorgnette::ScannerInfo> scanners_to_verify,
      lorgnette::ListScannersResponse list_response,
      GetScannerInfoListCallback callback,
      std::optional<lorgnette::OpenScannerResponse> open_response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    // This should only get called when there are scanners that need to be
    // verified.
    CHECK(!scanners_to_verify.empty());
    lorgnette::ScannerInfo scanner = std::move(scanners_to_verify.back());
    scanners_to_verify.pop_back();

    if (!open_response) {
      LOG(WARNING) << "Unable to open '" << scanner.name()
                   << "' while attempting to verify connectivity." << std::endl;
      VerifyScanners(std::move(client_id), std::move(scanners_to_verify),
                     std::move(list_response), std::move(callback));
      return;
    }

    // If the scanner was opened, close it.
    if (open_response->has_config()) {
      lorgnette::CloseScannerRequest close_request;
      *close_request.mutable_scanner() = open_response->config().scanner();
      GetLorgnetteManagerClient()->CloseScanner(
          close_request,
          base::BindOnce(&LorgnetteScannerManagerImpl::OnVerifyScannerClose,
                         weak_ptr_factory_.GetWeakPtr(), scanner.name()));
    }

    // If the result is success or busy (busy means the device is reachable but
    // another client is using it), insert this scanner into the response of
    // available scanners.
    if (open_response->result() == lorgnette::OPERATION_RESULT_SUCCESS ||
        open_response->result() == lorgnette::OPERATION_RESULT_DEVICE_BUSY) {
      verified_scanners_.insert(
          CreateScannerId(scanner.device_uuid(), scanner.name()));
      *list_response.add_scanners() = std::move(scanner);
    }

    VerifyScanners(std::move(client_id), std::move(scanners_to_verify),
                   std::move(list_response), std::move(callback));
  }

  // While verifying connectivity for a scanner, an open request is sent to the
  // scanner.  If that succeeds, a close request is sent to the scanner.  This
  // method is called in response to that close request.
  void OnVerifyScannerClose(
      const std::string& connection_string,
      std::optional<lorgnette::CloseScannerResponse> response) {
    if (!response ||
        response->result() != lorgnette::OPERATION_RESULT_SUCCESS) {
      LOG(WARNING) << "Unable to close scanner '" << connection_string
                   << "' while attempting to verify connectivity." << std::endl;
    }
  }

  // Instead of returning the raw SANE connection, give each client an
  // unguessable token representing the scanner.  This improves privacy by
  // removing IP addresses and USB serial numbers from the response.  In
  // addition, this makes it possible to return a new token when the SANE
  // connection string no longer refers to the same device (e.g., if the device
  // changes networks and a new scanner has the same IP as an old one).
  void UpdateScannerTokens(const std::string& client_id,
                           GetScannerInfoListCallback callback,
                           lorgnette::ListScannersResponse response) {
    TokenToScannerId& old_tokens = client_tokens_[client_id];
    TokenToScannerId new_tokens;

    // First ensure tokens are created for all newly-returned scanners.  If the
    // connection string and device UUID match a previously-returned scanner,
    // preserve the token value.
    //
    // This does a linear search of `old_tokens` for each new token, which takes
    // O(m*n) time.  This could be reduced by pre-parsing `old_tokens` into a
    // reverse map, but this isn't likely to be worth it because these lists are
    // expected to be very small in most cases.  Additionally, fetching the list
    // of scanners is already a very slow operation because it has to wait for
    // network responses and USB enumeration.
    for (lorgnette::ScannerInfo& scanner : *response.mutable_scanners()) {
      ScannerId new_id{scanner.device_uuid(), scanner.name()};
      std::string token;
      bool copied = false;
      for (auto& old : old_tokens) {
        if (old.second.has_value() && new_id == old.second.value()) {
          token = old.first;
          new_tokens.emplace(std::move(old));
          copied = true;
          break;
        }
      }
      if (!copied) {
        token = base::UnguessableToken::Create().ToString();
        new_tokens.emplace(token, new_id);
      }
      scanner.set_name(token);
    }

    // Create tombstones for any previously-returned tokens that are no longer
    // part of the response.
    for (const auto& [token, id] : old_tokens) {
      if (!base::Contains(new_tokens, token)) {
        new_tokens.emplace(token, std::nullopt);
      }
    }

    old_tokens.swap(new_tokens);
    std::move(callback).Run(std::move(response));
  }

  // Handles the result of calling LorgnetteManagerClient::ListScanners() for
  // GetScannerInfoList.
  void OnListScannerInfoResponse(
      const std::string& client_id,
      GetScannerInfoListCallback callback,
      LocalScannerFilter local_only,
      SecureScannerFilter secure_only,
      std::optional<lorgnette::ListScannersResponse> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

    // Combine zeroconf scanners and lorgnette scanners and send in callback.
    CreateCombinedScanners(std::move(client_id), local_only, secure_only,
                           response.value_or(lorgnette::ListScannersResponse()),
                           std::move(callback));
  }

  // Handles the result of calling
  // LorgnetteManagerClient::GetScannerCapabilities(). If getting the scanner
  // capabilities fails, |scanner_name|, |device_name|, and |protocol| are used
  // to mark the device name that was used as unusable and retry the
  // operation with the next available device name. This pattern of trying
  // each device name cannot be used when performing a scan since the backend
  // used to obtain the capabilities must be the same backend used to perform
  // the scan.
  void OnScannerCapabilitiesResponse(
      GetScannerCapabilitiesCallback callback,
      const std::string& scanner_name,
      const std::string& device_name,
      const ScanProtocol protocol,
      std::optional<lorgnette::ScannerCapabilities> capabilities) {
    if (!capabilities) {
      LOG(WARNING) << "Failed to get scanner capabilities using device name: "
                   << device_name;
      MarkDeviceNameUnusable(scanner_name, device_name, protocol);
      GetScannerCapabilities(scanner_name, std::move(callback));
      return;
    }

    PRINTER_LOG(DEBUG) << "Scanner capabilities for " << scanner_name << " at "
                       << device_name << " => "
                       << ScannerCapabilitiesToString(capabilities.value());

    std::move(callback).Run(capabilities);
  }

  void OnOpenScannerResponse(
      const lorgnette::ScannerId scanner_id,
      OpenScannerCallback callback,
      std::optional<lorgnette::OpenScannerResponse> response) {
    if (response) {
      PRINTER_LOG(EVENT) << "OpenScanner response received. Handle: "
                         << response->config().scanner().token();
      *response->mutable_scanner_id() = scanner_id;
    } else {
      PRINTER_LOG(ERROR) << "OpenScanner null response received.";
    }
    std::move(callback).Run(response);
  }

  void OnCloseScannerResponse(
      CloseScannerCallback callback,
      std::optional<lorgnette::CloseScannerResponse> response) {
    std::move(callback).Run(response);
  }

  void OnSetOptionsResponse(
      SetOptionsCallback callback,
      std::optional<lorgnette::SetOptionsResponse> response) {
    std::move(callback).Run(response);
  }

  void OnGetCurrentConfigResponse(
      GetCurrentConfigCallback callback,
      std::optional<lorgnette::GetCurrentConfigResponse> response) {
    std::move(callback).Run(response);
  }

  void OnStartPreparedScanResponse(
      StartPreparedScanCallback callback,
      std::optional<lorgnette::StartPreparedScanResponse> response) {
    std::move(callback).Run(response);
  }

  // Return true if |scanner| should be included in the results based on
  // |local_only| and |secure_only|, false if not.
  bool ShouldIncludeScanner(const lorgnette::ScannerInfo& scanner,
                            LocalScannerFilter local_only,
                            SecureScannerFilter secure_only) {
    if (local_only == LocalScannerFilter::kLocalScannersOnly &&
        scanner.connection_type() != lorgnette::CONNECTION_USB) {
      return false;
    }

    if (secure_only == SecureScannerFilter::kSecureScannersOnly &&
        !scanner.secure()) {
      return false;
    }

    return true;
  }

  // For a given `scanner` return a list of ScannerInfo objects.  One `scanner`
  // may have multiple device_names where each one corresponds to a new
  // ScannerInfo object.  `scanners_to_verify` is a list owned and provided by
  // the caller.  If a scanner needs to be verified for connectivity before
  // being returned to the caller, it will get inserted in this list instead of
  // the returned list.
  std::vector<lorgnette::ScannerInfo> CreateScannerInfosFromScanner(
      const Scanner& scanner,
      LocalScannerFilter local_only,
      SecureScannerFilter secure_only,
      std::vector<lorgnette::ScannerInfo>* scanners_to_verify) {
    CHECK(scanners_to_verify);
    std::vector<lorgnette::ScannerInfo> retval;

    // All ScannerInfo objects created from this scanner need to have the same
    // UUID.  If the scanner does not have a UUID, generate one to use.
    const std::string uuid =
        scanner.uuid.empty()
            ? base::Uuid::GenerateRandomV4().AsLowercaseString()
            : scanner.uuid;

    for (const auto& [protocol, device_names] : scanner.device_names) {
      for (const ScannerDeviceName& device_name : device_names) {
        if (!device_name.usable) {
          continue;
        }
        lorgnette::ConnectionType connection_type =
            lorgnette::CONNECTION_UNSPECIFIED;
        bool secure = false;
        bool need_to_verify = false;
        switch (protocol) {
          case (ScanProtocol::kEscl):
            connection_type = lorgnette::CONNECTION_NETWORK;
            secure = false;
            break;
          case (ScanProtocol::kEscls):
            connection_type = lorgnette::CONNECTION_NETWORK;
            secure = true;
            break;
          case (ScanProtocol::kLegacyNetwork):
            // These types of scanners need to have their connectivity verified
            // before they are returned to a client.
            connection_type = lorgnette::CONNECTION_NETWORK;
            secure = false;
            need_to_verify = !verified_scanners_.contains(
                CreateScannerId(uuid, device_name.device_name));
            break;
          case (ScanProtocol::kLegacyUsb):
            connection_type = lorgnette::CONNECTION_USB;
            secure = true;
            break;
          default:
            // Use defaults from above.
            break;
        }

        lorgnette::ScannerInfo info;
        info.set_name(device_name.device_name);
        info.set_manufacturer(scanner.manufacturer);
        info.set_model(scanner.model);
        info.set_display_name(scanner.display_name);
        // TODO(nmuggli): See if there's a way to determine the type of scanner.
        info.set_type("multi-function peripheral");
        info.set_device_uuid(uuid);
        info.set_connection_type(connection_type);
        info.set_secure(secure);
        // TODO(b/308191406): SANE backend only supports JPG and PNG, so
        // hardcode those for now.
        info.add_image_format("image/jpeg");
        info.add_image_format("image/png");
        info.set_protocol_type(ProtocolTypeForScanner(info));
        if (ShouldIncludeScanner(info, local_only, secure_only)) {
          if (need_to_verify) {
            scanners_to_verify->emplace_back(std::move(info));
          } else {
            retval.emplace_back(std::move(info));
          }
        }
      }
    }

    return retval;
  }

  // Use |response| and |zeroconf_scanners_| to build a combined
  // ListScannersResponse that will be sent in |callback|.  |local_only| and
  // |secure_only| are used to filter out network scanners and/or non-secure
  // scanners.
  void CreateCombinedScanners(const std::string& client_id,
                              LocalScannerFilter local_only,
                              SecureScannerFilter secure_only,
                              const lorgnette::ListScannersResponse& response,
                              GetScannerInfoListCallback callback) {
    lorgnette::ListScannersResponse combined_results;
    combined_results.set_result(response.result());

    for (const auto& scanner : response.scanners()) {
      if (!ShouldIncludeScanner(scanner, local_only, secure_only)) {
        continue;
      }

      lorgnette::ScannerInfo* scanner_out = combined_results.add_scanners();
      *scanner_out = scanner;

      for (Scanner& zeroconf_scanner : zeroconf_scanners_) {
        if (MergeDuplicateScannerRecords(scanner_out, zeroconf_scanner)) {
          PRINTER_LOG(DEBUG)
              << "Updating " << scanner.name() << ": " << scanner.display_name()
              << " -> " << scanner_out->display_name();
          break;
        }
      }
    }

    // Some of the zeroconf scanners may need to be verified before they can be
    // returned to the caller.  Keep track of those here.
    std::vector<lorgnette::ScannerInfo> scanners_to_verify;
    for (const Scanner& scanner : zeroconf_scanners_) {
      for (auto& info : CreateScannerInfosFromScanner(
               scanner, local_only, secure_only, &scanners_to_verify)) {
        *combined_results.add_scanners() = std::move(info);
      }
    }

    // For any of the non-escl network zeroconf scanners, make sure the scanner
    // is reachable before returning it to the user.
    VerifyScanners(std::move(client_id), std::move(scanners_to_verify),
                   std::move(combined_results), std::move(callback));
  }

  void OnReadScanDataResponse(
      ReadScanDataCallback callback,
      std::optional<lorgnette::ReadScanDataResponse> response) {
    std::move(callback).Run(response);
  }

  void OnCancelScanResponse(
      CancelScanCallback callback,
      std::optional<lorgnette::CancelScanResponse> response) {
    std::move(callback).Run(response);
  }

  // Uses |response| and zeroconf_scanners_ to rebuild deduped_scanners_.
  void RebuildDedupedScanners(
      std::optional<lorgnette::ListScannersResponse> response) {
    ResetDedupedScanners();
    ResetScannersToFilter();
    if (!response || response->scanners_size() == 0)
      return;

    // Iterate through each lorgnette scanner and add its info to an existing
    // Scanner if it has a matching IP address. Otherwise, create a new Scanner
    // for the lorgnette scanner.
    base::flat_map<net::IPAddress, std::string> known_ip_addresses =
        GetKnownIpAddresses();
    for (const auto& lorgnette_scanner : response->scanners()) {
      std::string ip_address_str;
      ScanProtocol protocol = ScanProtocol::kUnknown;
      ParseScannerName(lorgnette_scanner.name(), ip_address_str, protocol);
      if (!ip_address_str.empty()) {
        net::IPAddress ip_address;
        if (ip_address.AssignFromIPLiteral(ip_address_str)) {
          const auto it = known_ip_addresses.find(ip_address);
          if (it != known_ip_addresses.end()) {
            const auto existing = deduped_scanners_.find(it->second);
            DCHECK(existing != deduped_scanners_.end());
            existing->second.device_names[protocol].emplace(
                lorgnette_scanner.name());
            continue;
          }
        }
      }

      const bool is_usb_scanner = protocol == ScanProtocol::kLegacyUsb;
      const std::string base_name =
          CreateBaseName(lorgnette_scanner, is_usb_scanner);
      const std::string display_name = CreateUniqueDisplayName(base_name);

      Scanner scanner;
      scanner.display_name = display_name;
      scanner.device_names[protocol].emplace(lorgnette_scanner.name());
      deduped_scanners_[display_name] = scanner;
      scanners_to_filter_.push_back(display_name);
    }
  }

  // Resets |deduped_scanners_| by clearing it and repopulating it with
  // zeroconf_scanners_.
  void ResetDedupedScanners() {
    deduped_scanners_.clear();
    deduped_scanners_.reserve(zeroconf_scanners_.size());
    for (const auto& scanner : zeroconf_scanners_)
      deduped_scanners_[scanner.display_name] = scanner;
  }

  // Resets |scanners_to_filter| by clearing it and repopulating it with
  // zeroconf_scanners_ names.
  void ResetScannersToFilter() {
    scanners_to_filter_.clear();
    scanners_to_filter_.reserve(zeroconf_scanners_.size());
    for (const auto& scanner : zeroconf_scanners_)
      scanners_to_filter_.push_back(scanner.display_name);
  }

  // Returns a map of IP addresses to the display names (lookup keys) of
  // scanners they correspond to in deduped_scanners_. This enables
  // deduplication of network scanners by making it easy to check for and modify
  // them using their IP addresses.
  base::flat_map<net::IPAddress, std::string> GetKnownIpAddresses() {
    base::flat_map<net::IPAddress, std::string> known_ip_addresses;
    for (auto& entry : deduped_scanners_) {
      for (const auto& ip_address : entry.second.ip_addresses)
        known_ip_addresses[ip_address] = entry.second.display_name;
    }

    return known_ip_addresses;
  }

  // Creates a unique display name by appending a copy number to a duplicate
  // name (e.g. if Scanner Name already exists, the second instance will be
  // renamed Scanner Name (1)).
  std::string CreateUniqueDisplayName(const std::string& base_name) {
    std::string display_name = base_name;
    int i = 1;  // The first duplicate will become "Scanner Name (1)."
    while (deduped_scanners_.find(display_name) != deduped_scanners_.end()) {
      display_name = base::StringPrintf("%s (%d)", base_name.c_str(), i);
      i++;
    }

    return display_name;
  }

  // Gets the first usable device name corresponding to the highest priority
  // protocol for the scanner specified by |scanner_name|. Returns true on
  // success, false on failure.
  bool GetUsableDeviceNameAndProtocol(const std::string& scanner_name,
                                      std::string& device_name_out,
                                      ScanProtocol& protocol_out) {
    const auto scanner_it = deduped_scanners_.find(scanner_name);
    if (scanner_it == deduped_scanners_.end()) {
      PRINTER_LOG(ERROR) << "Failed to find scanner with name " << scanner_name;
      return false;
    }

    for (const auto& protocol : kPrioritizedProtocols) {
      const auto device_names_it =
          scanner_it->second.device_names.find(protocol);
      if (device_names_it == scanner_it->second.device_names.end())
        continue;

      for (const ScannerDeviceName& name : device_names_it->second) {
        if (name.usable) {
          device_name_out = name.device_name;
          protocol_out = protocol;
          return true;
        }
      }
    }

    PRINTER_LOG(ERROR) << "Failed to find usable device name for "
                       << scanner_name;
    return false;
  }

  // Marks a device name as unusable to prevent it from being returned by future
  // calls to GetUsableDeviceNameAndProtocol().
  void MarkDeviceNameUnusable(const std::string& scanner_name,
                              const std::string& device_name,
                              const ScanProtocol protocol) {
    auto scanner_it = deduped_scanners_.find(scanner_name);
    if (scanner_it == deduped_scanners_.end())
      return;

    auto device_names_it = scanner_it->second.device_names.find(protocol);
    if (device_names_it == scanner_it->second.device_names.end())
      return;

    for (ScannerDeviceName& name : device_names_it->second) {
      if (name.device_name == device_name) {
        name.usable = false;
        return;
      }
    }
  }

  // Used to detect zeroconf scanners.
  std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector_;

  // The deduplicated zeroconf scanners reported by the
  // zeroconf_scanner_detector_.
  std::vector<Scanner> zeroconf_scanners_;

  // Stores the deduplicated scanners from all sources in a map of display name
  // to Scanner. Clients are given display names and can use them to
  // interact with the corresponding scanners.
  base::flat_map<std::string, Scanner> deduped_scanners_;

  // Stores a list of scanner display names to check while filtering.
  std::vector<std::string> scanners_to_filter_;

  // Stores the UUID for zeroconf scanners that have already been verified.
  std::set<std::string> verified_scanners_;

  // For each client that has called GetScannerInfoList, maps scanner tokens
  // back to the original UUID and SANE connection string needed to open the
  // device.
  std::map<std::string, TokenToScannerId> client_tokens_;

  // Controls scanner notifications.
  std::unique_ptr<LorgnetteNotificationController>
      lorgnette_notification_controller_;

  SEQUENCE_CHECKER(sequence_);

  base::WeakPtrFactory<LorgnetteScannerManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<LorgnetteScannerManager> LorgnetteScannerManager::Create(
    std::unique_ptr<ZeroconfScannerDetector> zeroconf_scanner_detector,
    Profile* profile) {
  PRINTER_LOG(EVENT) << "LorgnetteScannerManager::Create";
  return std::make_unique<LorgnetteScannerManagerImpl>(
      std::move(zeroconf_scanner_detector), profile);
}

}  // namespace ash
