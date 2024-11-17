// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printers_manager.h"

#include <map>
#include <optional>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/printing/automatic_usb_printer_configurer.h"
#include "chrome/browser/ash/printing/cups_printer_status_creator.h"
#include "chrome/browser/ash/printing/enterprise/enterprise_printers_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_policy_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"
#include "chrome/browser/ash/printing/ppd_provider_factory.h"
#include "chrome/browser/ash/printing/ppd_resolution_tracker.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ash/printing/printer_event_tracker_factory.h"
#include "chrome/browser/ash/printing/printer_info.h"
#include "chrome/browser/ash/printing/printers_map.h"
#include "chrome/browser/ash/printing/server_printers_provider.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/printing/usb_printer_detector.h"
#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"
#include "chrome/browser/ash/printing/zeroconf_printer_detector.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printing_constants.h"
#include "chromeos/printing/uri.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/printer_query_result.h"

namespace ash {

constexpr base::TimeDelta kMetricsDelayTimerInterval = base::Minutes(1);
constexpr base::TimeDelta kMaxPrinterStatusPollingTime = base::Minutes(5);

enum class PollingIntervalLength {
  kShort = 0,
  kMedium = 1,
  kLong = 2,
  kMaxValue = kLong,
};

// Maps a PollingIntervalLength to a pair of numbers representing a range of
// seconds. The polling timer delay is chosen by randomly choosing a number
// between this range. Unreachable printers are more likely see a status change
// (by turning on or connecting to the network) so they should be queried more
// often.
constexpr auto kUnreachableStatePollingIntervalMap =
    base::MakeFixedFlatMap<PollingIntervalLength, std::pair<int, int>>(
        {{PollingIntervalLength::kShort, {10, 15}},
         {PollingIntervalLength::kMedium, {25, 30}},
         {PollingIntervalLength::kLong, {45, 60}}});
constexpr auto kGoodStatePollingIntervalMap =
    base::MakeFixedFlatMap<PollingIntervalLength, std::pair<int, int>>(
        {{PollingIntervalLength::kShort, {25, 30}},
         {PollingIntervalLength::kMedium, {60, 80}},
         {PollingIntervalLength::kLong, {60, 80}}});

bool IsIppUri(const chromeos::Uri& uri) {
  return (uri.GetScheme() == chromeos::kIppScheme ||
          uri.GetScheme() == chromeos::kIppsScheme);
}

// TODO(b/192467856) Remove this metric gathering by M99
void SendScannerCountToUMA(std::unique_ptr<ZeroconfScannerDetector> detector) {
  if (detector == nullptr) {
    PRINTER_LOG(DEBUG) << "SendScannerCountToUMA detector was null";
    return;
  }
  const uint16_t num_scanners = detector->GetScanners().size();
  base::UmaHistogramCounts100("Scanning.NumDetectedScannersAtLogin",
                              num_scanners);
}

namespace {

using ::chromeos::CupsPrinterStatus;
using ::chromeos::PpdProvider;
using ::chromeos::Printer;
using ::chromeos::PrinterClass;
using ::printing::PrinterQueryResult;

void OnRemovedPrinter(
    std::optional<printscanmgr::CupsRemovePrinterResponse> response) {
  if (!response) {
    PRINTER_LOG(DEBUG) << "No response to remove printer request.";
    return;
  }

  if (response->result()) {
    PRINTER_LOG(DEBUG) << "Printer removal succeeded.";
  } else {
    PRINTER_LOG(DEBUG) << "Printer removal failed.";
  }
}

class CupsPrintersManagerImpl
    : public CupsPrintersManager,
      public EnterprisePrintersProvider::Observer,
      public PrintServersManager::Observer,
      public SyncedPrintersManager::Observer,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  // Identifiers for each of the underlying PrinterDetectors this
  // class observes.
  enum DetectorIds { kUsbDetector, kZeroconfDetector, kPrintServerDetector };

  CupsPrintersManagerImpl(
      SyncedPrintersManager* synced_printers_manager,
      std::unique_ptr<PrinterDetector> usb_detector,
      std::unique_ptr<PrinterDetector> zeroconf_detector,
      scoped_refptr<PpdProvider> ppd_provider,
      DlcserviceClient* dlc_service_client,
      std::unique_ptr<UsbPrinterNotificationController>
          usb_notification_controller,
      std::unique_ptr<PrintServersManager> print_servers_manager,
      std::unique_ptr<EnterprisePrintersProvider> enterprise_printers_provider,
      PrinterEventTracker* event_tracker,
      PrefService* pref_service)
      : synced_printers_manager_(synced_printers_manager),
        usb_detector_(std::move(usb_detector)),
        zeroconf_detector_(std::move(zeroconf_detector)),
        ppd_provider_(std::move(ppd_provider)),
        dlc_service_client_(dlc_service_client),
        usb_notification_controller_(std::move(usb_notification_controller)),
        print_servers_manager_(std::move(print_servers_manager)),
        enterprise_printers_provider_(std::move(enterprise_printers_provider)),
        event_tracker_(event_tracker),
        nearby_printers_metric_delay_timer_(
            FROM_HERE,
            kMetricsDelayTimerInterval,
            /*receiver=*/this,
            &CupsPrintersManagerImpl::RecordTotalNearbyNetworkPrinterCounts) {
    auto_usb_printer_configurer_ =
        std::make_unique<AutomaticUsbPrinterConfigurer>(
            this, usb_notification_controller_.get(), ppd_provider_.get(),
            base::BindRepeating(&CupsPrintersManagerImpl::OnUsbPrinterSetupDone,
                                weak_ptr_factory_.GetWeakPtr()));

    GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());

    remote_cros_network_config_->AddObserver(
        cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

    // Prime the printer cache with the saved printers.
    printers_.ReplacePrintersInClass(
        PrinterClass::kSaved, synced_printers_manager_->GetSavedPrinters());
    synced_printers_manager_observation_.Observe(
        synced_printers_manager_.get());

    // Prime the printer cache with the enterprise printers (observer called
    // immediately).
    enterprise_printers_provider_observation_.Observe(
        enterprise_printers_provider_.get());

    // Callbacks may ensue immediately when the observer proxies are set up, so
    // these instantiations must come after everything else is initialized.
    usb_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kUsbDetector));
    OnPrintersFound(kUsbDetector, usb_detector_->GetPrinters());

    zeroconf_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kZeroconfDetector));
    OnPrintersFound(kZeroconfDetector, zeroconf_detector_->GetPrinters());

    // TODO(b/192467856) Remove this metric gathering by M99
    // Creates a ZeroconfScannerDetector, then logs the number of scanners
    // detected after 5 minutes.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SendScannerCountToUMA,
                       ZeroconfScannerDetector::Create()),
        base::Minutes(5));

    print_servers_manager_->AddObserver(this);

    user_printers_allowed_.Init(prefs::kUserPrintersAllowed, pref_service);
  }

  ~CupsPrintersManagerImpl() override = default;

  // Public API function.
  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!user_printers_allowed_.GetValue() &&
        printer_class != PrinterClass::kEnterprise) {
      // If printers are disabled then simply return an empty vector.
      LOG(WARNING) << "Attempting to retrieve printers when "
                      "UserPrintersAllowed is set to false";
      return {};
    }

    // Without user data there is not need to filter out non-enterprise or
    // insecure printers so return all the printers in |printer_class|.
    return printers_.Get(printer_class);
  }

  // Public API function.
  void SavePrinter(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!user_printers_allowed_.GetValue()) {
      LOG(WARNING) << "SavePrinter() called when "
                      "UserPrintersAllowed is set to false";
      return;
    }
    synced_printers_manager_->UpdateSavedPrinter(printer);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void RemoveSavedPrinter(const std::string& printer_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    UninstallPrinter(printer_id);
    auto existing = synced_printers_manager_->GetPrinter(printer_id);
    if (existing) {
      event_tracker_->RecordPrinterRemoved(*existing);
      const Printer::PrinterProtocol protocol = existing->GetProtocol();
      base::UmaHistogramEnumeration("Printing.CUPS.PrinterRemoved", protocol,
                                    Printer::PrinterProtocol::kProtocolMax);
    }
    synced_printers_manager_->RemoveSavedPrinter(printer_id);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void AddObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.AddObserver(observer);
    if (enterprise_printers_are_ready_) {
      observer->OnEnterprisePrintersInitialized();
    }
  }

  // Public API function.
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.RemoveObserver(observer);
  }

  // Public API function.
  void AddLocalPrintersObserver(
      CupsPrintersManager::LocalPrintersObserver* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

    if (!local_printers_observer_list_.HasObserver(observer)) {
      local_printers_observer_list_.AddObserver(observer);
      observer->OnLocalPrintersUpdated();
    }

    // Begin polling printers for printer status for 5 minutes.
    StartPrinterStatusPolling();
  }

  // Public API function.
  void RemoveLocalPrintersObserver(
      CupsPrintersManager::LocalPrintersObserver* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    local_printers_observer_list_.RemoveObserver(observer);
  }

  // Public API function.
  bool IsPrinterInstalled(const Printer& printer) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    const auto found = installed_printer_fingerprints_.find(printer.id());
    if (found == installed_printer_fingerprints_.end()) {
      return false;
    }

    return found->second == PrinterConfigurer::SetupFingerprint(printer);
  }

  // Public API function.
  std::optional<Printer> GetPrinter(const std::string& id) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!user_printers_allowed_.GetValue()) {
      LOG(WARNING) << "UserPrintersAllowed is disabled - only searching "
                      "enterprise printers";
      return GetEnterprisePrinter(id);
    }

    return printers_.Get(id);
  }

  // SyncedPrintersManager::Observer implementation
  void OnSavedPrintersChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    ResetNearbyPrintersLists();
    printers_.ReplacePrintersInClass(
        PrinterClass::kSaved, synced_printers_manager_->GetSavedPrinters());
    RebuildDetectedLists();
    NotifyObservers({PrinterClass::kSaved});
  }

  // EnterprisePrintersProvider::Observer implementation
  void OnPrintersChanged(bool complete,
                         const std::vector<Printer>& printers) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (complete) {
      printers_.ReplacePrintersInClass(PrinterClass::kEnterprise, printers);
      if (!enterprise_printers_are_ready_) {
        enterprise_printers_are_ready_ = true;
        for (auto& observer : observer_list_) {
          observer.OnEnterprisePrintersInitialized();
        }
      }
    }
    NotifyObservers({PrinterClass::kEnterprise});
  }

  // CrosNetworkConfigObserver implementation.
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override {
    if (!HasNetworkDisconnected(networks)) {
      // We only update the discovered list if we disconnected from our previous
      // default network.
      return;
    }

    PRINTER_LOG(DEBUG) << "Network change.  Refresh printers list.";

    // Clear the network detected printers when the active network changes.
    // This ensures that connecting to a new network will give us only newly
    // detected printers.
    ClearNetworkDetectedPrinters();

    // Notify observers that the printer list has changed.
    RebuildDetectedLists();
  }

  // Callback for PrinterDetectors.
  void OnPrintersFound(
      int detector_id,
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    switch (detector_id) {
      case kUsbDetector:
        usb_detections_ = printers;
        break;
      case kZeroconfDetector:
        zeroconf_detections_ = printers;
        // Start timer for recording the # of nearby printers.
        nearby_printers_metric_delay_timer_.Reset();
        break;
      case kPrintServerDetector:
        servers_detections_ = printers;
        break;
    }
    RebuildDetectedLists();
  }

  // Callback for PrintServersManager.
  void OnServerPrintersChanged(
      const std::vector<PrinterDetector::DetectedPrinter>& printers) override {
    OnPrintersFound(kPrintServerDetector, printers);
  }

  void SetUpPrinter(const chromeos::Printer& printer,
                    bool is_automatic_installation,
                    PrinterSetupCallback callback) override {
    // Check if the printer is currently set up.
    if (IsPrinterInstalled(printer)) {
      std::move(callback).Run(PrinterSetupResult::kSuccess);
      return;
    }

    const std::string id = printer.id();
    const std::string fingerprint =
        PrinterConfigurer::SetupFingerprint(printer);

    // Add `callback` to an existing record or creates a new record with
    // an empty fingerprint and a single callback.
    printers_being_setup_[id].callbacks.push_back(std::move(callback));

    // If the record was just created or the fingerprint does not match the
    // previous one, we have to initialize/overwrite the rest of the fields and
    // start/restart the setup process.
    if (printers_being_setup_[id].fingerprint != fingerprint) {
      printers_being_setup_[id].configurer =
          PrinterConfigurer::Create(ppd_provider_, dlc_service_client_);
      printers_being_setup_[id].fingerprint = fingerprint;
      printers_being_setup_[id].configurer->SetUpPrinterInCups(
          printer,
          base::BindOnce(&CupsPrintersManagerImpl::OnPrinterSetupResult,
                         weak_ptr_factory_.GetWeakPtr(), id,
                         is_automatic_installation));
    }
  }

  void UninstallPrinter(const std::string& printer_id) override {
    // Uninstall printer if installed completely.
    if (installed_printer_fingerprints_.erase(printer_id)) {
      // The printer was present in `installed_printer_fingerprints_`.
      printscanmgr::CupsRemovePrinterRequest request;
      request.set_name(printer_id);
      PrintscanmgrClient::Get()->CupsRemovePrinter(
          request, base::BindOnce(&OnRemovedPrinter), base::DoNothing());
      return;
    }

    // If the printer is being installed now, stop the process.
    std::vector<PrinterSetupCallback> callbacks;
    if (printers_being_setup_.contains(printer_id)) {
      callbacks = std::move(printers_being_setup_[printer_id].callbacks);
      printers_being_setup_.erase(printer_id);
    }
    for (auto& callback : callbacks) {
      std::move(callback).Run(PrinterSetupResult::kPrinterRemoved);
    }
  }

  // Resets the overall polling timer then executes the first round of printer
  // status queries for good and unreachable printers.
  void StartPrinterStatusPolling() {
    printer_status_polling_total_duration_timer_ =
        std::make_unique<base::ElapsedTimer>();
    OnPrinterStatusTimerElapsed(/*for_unreachable_printers=*/true);
    OnPrinterStatusTimerElapsed(/*for_unreachable_printers=*/false);
  }

  // Determines if a printer is unreachable based on it's previously acquired
  // printer status.
  bool IsPrinterUnreachable(const chromeos::Printer& printer) {
    const auto printer_status = printer.printer_status();
    for (const auto& reason : printer.printer_status().GetStatusReasons()) {
      if (reason.GetReason() == CupsPrinterStatus::CupsPrinterStatusReason::
                                    Reason::kPrinterUnreachable) {
        return true;
      }
    }

    return false;
  }

  // Returns the next polling delay in seconds based on the state of the
  // printers, the # of printers being queried, and total polling time elapsed.
  int GetPrinterStatusPollingDelay(bool for_unreachable_printers,
                                   int printers_queried) {
    // After polling for 2 minutes the printers' statuses are less likely to
    // change so increase the polling delay.
    const base::TimeDelta kLongDuration = base::Minutes(2);
    // If there are large number of printers to query, increase the polling
    // delay to reduce the overall use of network bandwidth.
    const int kMaxPrinters = 3;

    // Unreachable printers are more likely to have their status changed (by
    // being turned on and connecting to the network) so they should be
    // queried more often.
    const auto& polling_intervals = for_unreachable_printers
                                        ? kUnreachableStatePollingIntervalMap
                                        : kGoodStatePollingIntervalMap;
    std::pair<int, int> interval;
    if (printer_status_polling_total_duration_timer_->Elapsed() >
        kLongDuration) {
      interval = polling_intervals.find(PollingIntervalLength::kLong)->second;
    } else if (printers_queried > kMaxPrinters) {
      interval = polling_intervals.find(PollingIntervalLength::kMedium)->second;
    } else {
      interval = polling_intervals.find(PollingIntervalLength::kShort)->second;
    }

    // Choose a random int between the selected interval.
    return interval.first + (rand() % (interval.second - interval.first + 1));
  }

  // Starts printer status requests for all Saved and recently used printers
  // then queues the next round of requests if the overall timer hasn't elapsed.
  // `for_unreachable_printers` determines when set of polling intervals to use.
  void OnPrinterStatusTimerElapsed(bool for_unreachable_printers) {
    std::vector<std::string> recently_used_printers;
    ::printing::PrintPreviewStickySettings* sticky_settings =
        ::printing::PrintPreviewStickySettings::GetInstance();
    if (sticky_settings) {
      recently_used_printers = sticky_settings->GetRecentlyUsedPrinters();
    }

    int printers_queried = 0;
    const auto printers = printers_.Get();
    for (const auto& printer : printers) {
      // Ensure the correct set of printers is being queried.
      if (IsPrinterUnreachable(printer) != for_unreachable_printers) {
        continue;
      }

      // Query every printer that is either saved or recently used.
      if (printers_.IsPrinterInClass(chromeos::PrinterClass::kSaved,
                                     printer.id()) ||
          base::Contains(recently_used_printers, printer.id())) {
        FetchPrinterStatus(printer.id(),
                           /*PrinterStatusCallback=*/base::DoNothing());
        ++printers_queried;
      }
    }

    // Only restart requests when the 5 minute timer hasn't elapsed.
    if (printer_status_polling_total_duration_timer_->Elapsed() <
        kMaxPrinterStatusPollingTime) {
      auto& timer = for_unreachable_printers
                        ? printer_status_unreachable_state_timer_
                        : printer_status_good_state_timer_;
      timer.Start(
          FROM_HERE,
          base::Seconds(GetPrinterStatusPollingDelay(for_unreachable_printers,
                                                     printers_queried)),
          base::BindOnce(&CupsPrintersManagerImpl::OnPrinterStatusTimerElapsed,
                         weak_ptr_factory_.GetWeakPtr(),
                         for_unreachable_printers));
    }
  }

  void FetchPrinterStatus(const std::string& printer_id,
                          PrinterStatusCallback cb) override {
    std::optional<Printer> printer = GetPrinter(printer_id);
    if (!printer) {
      PRINTER_LOG(ERROR) << printer_id
                         << ": Unable to complete printer status request: "
                         << "GetPrinter failed.";
      CupsPrinterStatus printer_status(printer_id);
      printer_status.AddStatusReason(
          CupsPrinterStatus::CupsPrinterStatusReason::Reason::
              kPrinterUnreachable,
          CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
      SendPrinterStatus(printer_status, std::move(cb),
                        /*notify_observers=*/false);
      return;
    }

    // For USB printers, return NO ERROR if the printer is connected or PRINTER
    // UNREACHABLE if the printer is disconnected.
    if (printer->IsUsbProtocol()) {
      CupsPrinterStatus printer_status(printer_id);
      if (FindDetectedPrinter(printer_id)) {
        printer_status.AddStatusReason(
            CupsPrinterStatus::CupsPrinterStatusReason::Reason::kNoError,
            CupsPrinterStatus::CupsPrinterStatusReason::Severity::
                kUnknownSeverity);
      } else {
        printer_status.AddStatusReason(
            CupsPrinterStatus::CupsPrinterStatusReason::Reason::
                kPrinterUnreachable,
            CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
      }
      SendPrinterStatus(printer_status, std::move(cb),
                        /*notify_observers=*/true);
      return;
    }

    // Behavior for querying a non-IPP uri is undefined and disallowed.
    if (!IsIppUri(printer->uri())) {
      PRINTER_LOG(DEBUG) << printer_id
                         << ": Cannot send status request to non-IPP URI for "
                         << printer->make_and_model() << ": "
                         << printer->uri().GetNormalized(
                                /*always_include_port=*/true);
      CupsPrinterStatus printer_status(printer_id);
      printer_status.AddStatusReason(
          CupsPrinterStatus::CupsPrinterStatusReason::Reason::kUnknownReason,
          CupsPrinterStatus::CupsPrinterStatusReason::Severity::kWarning);
      SendPrinterStatus(printer_status, std::move(cb),
                        /*notify_observers=*/false);
      return;
    }

    PRINTER_LOG(DEBUG) << printer_id << ": Sending status request for "
                       << printer->make_and_model() << ": "
                       << printer->uri().GetNormalized(
                              /*always_include_port=*/true);
    QueryIppPrinter(
        printer->uri().GetHostEncoded(), printer->uri().GetPort(),
        printer->uri().GetPathEncodedAsString(),
        printer->uri().GetScheme() == chromeos::kIppsScheme,
        base::BindOnce(&CupsPrintersManagerImpl::OnPrinterInfoFetched,
                       weak_ptr_factory_.GetWeakPtr(), printer_id,
                       std::move(cb)));
  }

  // Public API function.
  void RecordNearbyNetworkPrinterCounts() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

    size_t total_network_printers_count = zeroconf_detections_.size();
    // Count detected network printers that have not been saved
    size_t nearby_zeroconf_printers_count = 0;
    for (const PrinterDetector::DetectedPrinter& detected :
         zeroconf_detections_) {
      if (!printers_.IsPrinterInClass(PrinterClass::kSaved,
                                      detected.printer.id())) {
        ++nearby_zeroconf_printers_count;
      }
    }

    base::UmaHistogramCounts100(
        "Printing.CUPS.TotalNetworkPrintersCount2.SettingsOpened",
        total_network_printers_count);
    base::UmaHistogramCounts100("Printing.CUPS.NearbyNetworkPrintersCount",
                                nearby_zeroconf_printers_count);
  }

  PrintServersManager* GetPrintServersManager() const override {
    return print_servers_manager_.get();
  }

  // Callback for FetchPrinterStatus
  void OnPrinterInfoFetched(
      const std::string& printer_id,
      PrinterStatusCallback cb,
      PrinterQueryResult result,
      const ::printing::PrinterStatus& printer_status,
      const std::string& make_and_model,
      const std::vector<std::string>& document_formats,
      bool ipp_everywhere,
      const chromeos::PrinterAuthenticationInfo& auth_info) {
    ParsePrinterStatusFromPrinterQuery(printer_id, std::move(cb), result,
                                       printer_status, auth_info);
  }

  void ParsePrinterStatusFromPrinterQuery(
      const std::string& printer_id,
      PrinterStatusCallback cb,
      PrinterQueryResult result,
      const ::printing::PrinterStatus& printer_status,
      const chromeos::PrinterAuthenticationInfo& auth_info) {
    base::UmaHistogramEnumeration("Printing.CUPS.PrinterStatusQueryResult",
                                  result);
    switch (result) {
      case PrinterQueryResult::kHostnameResolution:
      case PrinterQueryResult::kUnreachable: {
        PRINTER_LOG(ERROR)
            << printer_id
            << ": Printer status request failed. Could not reach printer: "
            << (result == PrinterQueryResult::kHostnameResolution
                    ? "hostname resolution failed"
                    : "device unreachable");
        CupsPrinterStatus error_printer_status(printer_id);
        error_printer_status.AddStatusReason(
            CupsPrinterStatus::CupsPrinterStatusReason::Reason::
                kPrinterUnreachable,
            CupsPrinterStatus::CupsPrinterStatusReason::Severity::kError);
        SendPrinterStatus(error_printer_status, std::move(cb),
                          /*notify_observers=*/true);
        break;
      }
      case PrinterQueryResult::kUnknownFailure: {
        PRINTER_LOG(ERROR) << printer_id
                           << ": Printer status request failed. Unknown "
                              "failure trying to reach printer";
        CupsPrinterStatus error_printer_status(printer_id);
        error_printer_status.AddStatusReason(
            CupsPrinterStatus::CupsPrinterStatusReason::Reason::kUnknownReason,
            CupsPrinterStatus::CupsPrinterStatusReason::Severity::kWarning);
        SendPrinterStatus(error_printer_status, std::move(cb),
                          /*notify_observers=*/true);
        break;
      }
      case PrinterQueryResult::kSuccess: {
        // Record results from PrinterStatus before converting to
        // CupsPrinterStatus because the PrinterStatus enum contains more reason
        // buckets.
        for (const auto& reason : printer_status.reasons) {
          base::UmaHistogramEnumeration("Printing.CUPS.PrinterStatusReasons",
                                        reason.reason);
        }

        // Convert printing::PrinterStatus to printing::CupsPrinterStatus
        CupsPrinterStatus cups_printers_status =
            PrinterStatusToCupsPrinterStatus(printer_id, printer_status,
                                             auth_info);

        // Send status back to the handler through PrinterStatusCallback.
        SendPrinterStatus(cups_printers_status, std::move(cb),
                          /*notify_observers=*/true);
        break;
      }
    }
  }

  // Sends the printer status via callback then notifies the local printer
  // observers.
  void SendPrinterStatus(CupsPrinterStatus printer_status,
                         PrinterStatusCallback cb,
                         bool notify_observers) {
    if (notify_observers) {
      // Save the status so it can be attached with the printer for future
      // retrievals.
      const bool is_new_status = printers_.SavePrinterStatus(
          printer_status.GetPrinterId(), printer_status);
      if (is_new_status) {
        NotifyLocalPrinterObservers();
      }
    }
    std::move(cb).Run(std::move(printer_status));
  }

  void QueryPrinterForAutoConf(
      const Printer& printer,
      base::OnceCallback<void(bool)> callback) override {
    if (!IsIppUri(printer.uri())) {
      std::move(callback).Run(false);
      return;
    }

    QueryIppPrinter(
        printer.uri().GetHostEncoded(), printer.uri().GetPort(),
        printer.uri().GetPathEncodedAsString(),
        printer.uri().GetScheme() == chromeos::kIppsScheme,
        base::BindOnce(&CupsPrintersManagerImpl::OnQueryPrinterForAutoConf,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Callback for QueryPrinterForAutoConf
  void OnQueryPrinterForAutoConf(
      base::OnceCallback<void(bool)> callback,
      PrinterQueryResult result,
      const ::printing::PrinterStatus& printer_status,
      const std::string& make_and_model,
      const std::vector<std::string>& document_formats,
      bool ipp_everywhere,
      const chromeos::PrinterAuthenticationInfo& auth_info) {
    if (result != PrinterQueryResult::kSuccess) {
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(ipp_everywhere);
  }

 private:
  std::optional<Printer> GetEnterprisePrinter(const std::string& id) const {
    return printers_.Get(PrinterClass::kEnterprise, id);
  }

  // TODO(baileyberro): Remove the need for this function by pushing additional
  // logic into PrintersMap. https://crbug.com/956172
  void ResetNearbyPrintersLists() {
    printers_.Clear(PrinterClass::kAutomatic);
    printers_.Clear(PrinterClass::kDiscovered);
  }

  // Notify observers on the given classes the the relevant lists have changed.
  void NotifyObservers(const std::vector<PrinterClass>& printer_classes) {
    for (auto printer_class : printer_classes) {
      auto printers = printers_.Get(printer_class);
      PRINTER_LOG(DEBUG) << "Sending notification for " << printers.size()
                         << " printers in class (" << ToString(printer_class)
                         << ")";
      for (auto& observer : observer_list_) {
        observer.OnPrintersChanged(printer_class, printers);
      }
    }
  }

  // Notify observers that a local printer has updated.
  void NotifyLocalPrinterObservers() {
    for (auto& observer : local_printers_observer_list_) {
      observer.OnLocalPrintersUpdated();
    }
  }

  // Look through all sources for the detected printer with the given id.
  // Return a pointer to the printer on found, null if no entry is found.
  const PrinterDetector::DetectedPrinter* FindDetectedPrinter(
      const std::string& id) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const auto* printer_list : {&usb_detections_, &zeroconf_detections_}) {
      for (const auto& detected : *printer_list) {
        if (detected.printer.id() == id) {
          return &detected;
        }
      }
    }
    return nullptr;
  }

  void MaybeRecordInstallation(const Printer& printer,
                               bool is_automatic_installation) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (synced_printers_manager_->GetPrinter(printer.id())) {
      // It's just an update, not a new installation, so don't record an event.
      return;
    }

    // For compatibility with the previous implementation, record USB printers
    // separately from other IPP printers.  Eventually we may want to shift
    // this to be split by autodetected/not autodetected instead of USB/other
    // IPP.
    if (printer.IsUsbProtocol()) {
      // Get the associated detection record if one exists.
      const auto* detected = FindDetectedPrinter(printer.id());
      // We should have the full DetectedPrinter.  We can't log the printer if
      // we don't have it.
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for installation event logging";
        return;
      }
      // For recording purposes, this is an automatic install if the ppd
      // reference generated at detection time is the is the one we actually
      // used -- i.e. the user didn't have to change anything to obtain a ppd
      // that worked.
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordUsbPrinterInstalled(*detected, mode);
    } else {
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordIppPrinterInstalled(printer, mode);
    }
  }

  void AddDetectedUsbPrinters(
      const std::vector<PrinterDetector::DetectedPrinter>& detected_list) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

    // Update the list of connected printers (skip the saved ones).
    std::vector<PrinterDetector::DetectedPrinter> printers;
    for (const PrinterDetector::DetectedPrinter& detected : detected_list) {
      if (!printers_.IsPrinterInClass(PrinterClass::kSaved,
                                      detected.printer.id())) {
        printers.push_back(detected);
      }
    }
    auto_usb_printer_configurer_->UpdateListOfConnectedPrinters(
        std::move(printers));

    // Update lists of Automatic and Discovered printers.
    for (const std::string& printer_id :
         auto_usb_printer_configurer_->ConfiguredPrintersIds()) {
      if (!printers_.IsPrinterInClass(PrinterClass::kAutomatic, printer_id)) {
        AddPrinterToPrintersMap(
            PrinterClass::kAutomatic,
            auto_usb_printer_configurer_->Printer(printer_id));
      }
    }
    for (const std::string& printer_id :
         auto_usb_printer_configurer_->UnconfiguredPrintersIds()) {
      if (!printers_.IsPrinterInClass(PrinterClass::kDiscovered, printer_id)) {
        AddPrinterToPrintersMap(
            PrinterClass::kDiscovered,
            auto_usb_printer_configurer_->Printer(printer_id));
      }
    }
  }

  void AddDetectedNetworkPrinters(
      const std::vector<PrinterDetector::DetectedPrinter>& detected_list) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const PrinterDetector::DetectedPrinter& detected : detected_list) {
      const std::string& detected_printer_id = detected.printer.id();
      if (printers_.IsPrinterInClass(PrinterClass::kSaved,
                                     detected_printer_id)) {
        // It's already in the saved class, don't need to do anything else here.
        continue;
      }

      // Sometimes the detector can flag a printer as IPP-everywhere compatible;
      // those printers can go directly into the automatic class without further
      // processing.
      auto printer = detected.printer;
      if (printer.IsIppEverywhere()) {
        AddPrinterToPrintersMap(PrinterClass::kAutomatic, printer);
        continue;
      }

      if (!ppd_resolution_tracker_.IsResolutionComplete(detected_printer_id)) {
        // Didn't find an entry for this printer in the PpdReferences cache.  We
        // need to ask PpdProvider whether or not it can determine a
        // PpdReference.  If there's not already an outstanding request for one,
        // start one.  When the request comes back, we'll rerun classification
        // and then should be able to figure out where this printer belongs.
        if (!ppd_resolution_tracker_.IsResolutionPending(detected_printer_id)) {
          ppd_resolution_tracker_.MarkResolutionPending(detected_printer_id);
          ppd_provider_->ResolvePpdReference(
              detected.ppd_search_data,
              base::BindOnce(&CupsPrintersManagerImpl::ResolvePpdReferenceDone,
                             weak_ptr_factory_.GetWeakPtr(),
                             detected_printer_id));
        }
        continue;
      }
      if (ppd_resolution_tracker_.WasResolutionSuccessful(
              detected_printer_id)) {
        // We have a ppd reference, so we think we can set this up
        // automatically.
        *printer.mutable_ppd_reference() =
            ppd_resolution_tracker_.GetPpdReference(detected_printer_id);
        AddPrinterToPrintersMap(PrinterClass::kAutomatic, printer);
        continue;
      }

      // We are not able to set the printer up automatically.
      AddPrinterToPrintersMap(PrinterClass::kDiscovered, printer);
    }
  }

  void AddPrinterToPrintersMap(PrinterClass printer_class,
                               const Printer& printer) {
    printers_.Insert(printer_class, printer);

    // If we've seen this printer before, don't trigger a new detection event.
    if (detected_printers_seen_.contains(printer.id())) {
      return;
    }

    detected_printers_seen_.insert(printer.id());
    NotifyLocalPrinterObservers();
  }

  // Returns true if we've disconnected from our current network. Updates
  // the current active network. This method is not reentrant.
  bool HasNetworkDisconnected(
      const std::vector<
          chromeos::network_config::mojom::NetworkStatePropertiesPtr>&
          networks) {
    // An empty current_network indicates that we're not connected to a valid
    // network right now.
    std::string current_network;
    if (!networks.empty()) {
      // The first network is the default network which receives mDNS
      // multicasts.
      current_network = networks.front()->guid;
    }

    // If we attach to a network after being disconnected, we do not want to
    // forcibly clear our detected list.  It is either already empty or contains
    // valid entries because we missed the original connection event.
    bool network_disconnected =
        !active_network_.empty() && current_network != active_network_;

    // Ensure that we don't register network state updates as network changes.
    active_network_ = std::move(current_network);

    return network_disconnected;
  }

  // Record in UMA the appropriate event with a setup attempt for a printer is
  // abandoned.
  void RecordSetupAbandoned(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (printer.IsUsbProtocol()) {
      const auto* detected = FindDetectedPrinter(printer.id());
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for abandoned event logging";
        return;
      }
      event_tracker_->RecordUsbSetupAbandoned(*detected);
    } else {
      event_tracker_->RecordSetupAbandoned(printer);
    }
  }

  // Rebuild the Automatic and Discovered printers lists from the (cached) raw
  // detections.  This will also generate OnPrintersChanged events for any
  // observers observering either of the detected lists (kAutomatic and
  // kDiscovered).
  void RebuildDetectedLists() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    ResetNearbyPrintersLists();
    AddDetectedUsbPrinters(usb_detections_);
    AddDetectedNetworkPrinters(zeroconf_detections_);
    AddDetectedNetworkPrinters(servers_detections_);
    NotifyObservers({PrinterClass::kAutomatic, PrinterClass::kDiscovered});
  }

  void OnUsbPrinterSetupDone(std::string printer_id) {
    if (auto_usb_printer_configurer_->ConfiguredPrintersIds().contains(
            printer_id)) {
      AddPrinterToPrintersMap(
          PrinterClass::kAutomatic,
          auto_usb_printer_configurer_->Printer(printer_id));
      NotifyObservers({PrinterClass::kAutomatic});
    } else {
      AddPrinterToPrintersMap(
          PrinterClass::kDiscovered,
          auto_usb_printer_configurer_->Printer(printer_id));
      NotifyObservers({PrinterClass::kDiscovered});
    }
  }

  // Callback invoked on completion of PpdProvider::ResolvePpdReference.
  void ResolvePpdReferenceDone(const std::string& printer_id,
                               PpdProvider::CallbackResultCode code,
                               const Printer::PpdReference& ref,
                               const std::string& usb_manufacturer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (code == PpdProvider::SUCCESS) {
      ppd_resolution_tracker_.MarkResolutionSuccessful(printer_id, ref);
    } else {
      LOG(WARNING) << printer_id << ": Failed to resolve PPD reference: "
                   << PpdProvider::CallbackResultCodeName(code);
      ppd_resolution_tracker_.MarkResolutionFailed(printer_id);
      if (!usb_manufacturer.empty()) {
        ppd_resolution_tracker_.SetManufacturer(printer_id, usb_manufacturer);
      }
    }
    RebuildDetectedLists();
  }

  // Callback for `SetUpPrinterInCups`.
  void OnPrinterSetupResult(const std::string& printer_id,
                            bool is_automatic_installation,
                            PrinterSetupResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);

    std::map<std::string, PrinterSetupTracker>::iterator it =
        printers_being_setup_.find(printer_id);
    DCHECK(it != printers_being_setup_.end());

    if (result == PrinterSetupResult::kSuccess) {
      installed_printer_fingerprints_[printer_id] = it->second.fingerprint;
      // TODO: b/295243026 - Solve this issue during metrics clean-up.
      // We check this condition before calling MaybeRecordInstallation() to
      // make it backward compatible with the state before crrev.com/c/4763464.
      // MaybeRecordInstallation() is used only for reporting and changing the
      // condition below may have significant influence on some metrics.
      // The better solution would be, instead of checking this flag, to NOT
      // record events for server and enterprise printers.
      if (user_printers_allowed_.GetValue()) {
        std::optional<chromeos::Printer> printer = printers_.Get(printer_id);
        if (printer) {
          MaybeRecordInstallation(*printer, is_automatic_installation);
        }
      }
    }

    std::vector<PrinterSetupCallback> callbacks =
        std::move(it->second.callbacks);
    printers_being_setup_.erase(it);

    for (auto& callback : callbacks) {
      std::move(callback).Run(result);
    }
  }

  // Resets all network detected printer lists.
  void ClearNetworkDetectedPrinters() {
    PRINTER_LOG(DEBUG) << "Clear network printers";
    zeroconf_detections_.clear();

    ResetNearbyPrintersLists();
  }

  void RecordTotalNearbyNetworkPrinterCounts() {
    base::UmaHistogramCounts100("Printing.CUPS.TotalNetworkPrintersCount2",
                                zeroconf_detections_.size());
  }

  SEQUENCE_CHECKER(sequence_);

  // Source lists for detected printers.
  std::vector<PrinterDetector::DetectedPrinter> usb_detections_;
  std::vector<PrinterDetector::DetectedPrinter> zeroconf_detections_;
  std::vector<PrinterDetector::DetectedPrinter> servers_detections_;

  // Not owned.
  const raw_ptr<SyncedPrintersManager> synced_printers_manager_;
  base::ScopedObservation<SyncedPrintersManager,
                          SyncedPrintersManager::Observer>
      synced_printers_manager_observation_{this};
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  std::unique_ptr<PrinterDetector> usb_detector_;

  std::unique_ptr<PrinterDetector> zeroconf_detector_;

  scoped_refptr<PpdProvider> ppd_provider_;
  raw_ptr<DlcserviceClient> dlc_service_client_;

  std::unique_ptr<UsbPrinterNotificationController>
      usb_notification_controller_;

  std::unique_ptr<AutomaticUsbPrinterConfigurer> auto_usb_printer_configurer_;

  std::unique_ptr<PrintServersManager> print_servers_manager_;

  std::unique_ptr<EnterprisePrintersProvider> enterprise_printers_provider_;
  base::ScopedObservation<EnterprisePrintersProvider,
                          EnterprisePrintersProvider::Observer>
      enterprise_printers_provider_observation_{this};

  // Not owned
  const raw_ptr<PrinterEventTracker> event_tracker_;

  // Categorized printers.  This is indexed by PrinterClass.
  PrintersMap printers_;

  // Equals true if the list of enterprise printers and related policies
  // is initialized and configured correctly.
  bool enterprise_printers_are_ready_ = false;

  // GUID of the current default network.
  std::string active_network_;

  // Tracks PpdReference resolution. Also stores USB manufacturer string if
  // available.
  PpdResolutionTracker ppd_resolution_tracker_;

  // Map of printer ids to PrinterConfigurer setup fingerprints at the time
  // the printers was last installed with CUPS.
  std::map<std::string, std::string> installed_printer_fingerprints_;

  // List of printers being currently setup in CUPS.
  struct PrinterSetupTracker {
    std::unique_ptr<PrinterConfigurer> configurer;
    std::string fingerprint;
    std::vector<PrinterSetupCallback> callbacks;
  };
  std::map<std::string, PrinterSetupTracker> printers_being_setup_;

  base::ObserverList<CupsPrintersManager::Observer>::Unchecked observer_list_;

  // Maintains list of observers for updates to local printers.
  base::ObserverList<CupsPrintersManager::LocalPrintersObserver>
      local_printers_observer_list_;

  // Holds the current value of the pref |UserPrintersAllowed|.
  BooleanPrefMember user_printers_allowed_;

  // Timer used to prevent the total nearby printers from immediately recording
  // each time the mDNS reports printers.
  base::DelayTimer nearby_printers_metric_delay_timer_;

  // Tracks the printers seen from mDNS or USB plug ins so the
  // LocalPrinterObserver knows when to fire for a new printer.
  // TODO(b/304269962): Remove detected printers from here when disconnected
  // from the device.
  base::flat_set<std::string> detected_printers_seen_;

  // Once elapsed, executes a round of printer status queries.
  base::OneShotTimer printer_status_good_state_timer_;
  base::OneShotTimer printer_status_unreachable_state_timer_;

  // Used to limit the total duration of printer status polling.
  std::unique_ptr<base::ElapsedTimer>
      printer_status_polling_total_duration_timer_;

  base::WeakPtrFactory<CupsPrintersManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::Create(
    Profile* profile) {
  return std::make_unique<CupsPrintersManagerImpl>(
      SyncedPrintersManagerFactory::GetInstance()->GetForBrowserContext(
          profile),
      UsbPrinterDetector::Create(), ZeroconfPrinterDetector::Create(),
      CreatePpdProvider(profile), DlcserviceClient::Get(),
      UsbPrinterNotificationController::Create(profile),
      PrintServersManager::Create(profile),
      EnterprisePrintersProvider::Create(CrosSettings::Get(), profile),
      PrinterEventTrackerFactory::GetInstance()->GetForBrowserContext(profile),
      profile->GetPrefs());
}

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::CreateForTesting(
    SyncedPrintersManager* synced_printers_manager,
    std::unique_ptr<PrinterDetector> usb_detector,
    std::unique_ptr<PrinterDetector> zeroconf_detector,
    scoped_refptr<PpdProvider> ppd_provider,
    DlcserviceClient* dlc_service_client,
    std::unique_ptr<UsbPrinterNotificationController>
        usb_notification_controller,
    std::unique_ptr<PrintServersManager> print_servers_manager,
    std::unique_ptr<EnterprisePrintersProvider> enterprise_printers_provider,
    PrinterEventTracker* event_tracker,
    PrefService* pref_service) {
  return std::make_unique<CupsPrintersManagerImpl>(
      synced_printers_manager, std::move(usb_detector),
      std::move(zeroconf_detector), std::move(ppd_provider), dlc_service_client,
      std::move(usb_notification_controller), std::move(print_servers_manager),
      std::move(enterprise_printers_provider), event_tracker, pref_service);
}

// static
void CupsPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUserPrintersAllowed, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(prefs::kPrintingSendUsernameAndFilenameEnabled,
                                false);
  PrintServersProvider::RegisterProfilePrefs(registry);
}

// static
void CupsPrintersManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  PrintServersProvider::RegisterLocalStatePrefs(registry);
  printing::oauth2::ClientIdsDatabase::RegisterLocalStatePrefs(registry);
}

}  // namespace ash
