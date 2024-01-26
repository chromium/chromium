// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <cmath>
#include <limits>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"
#include "chrome/browser/extensions/api/document_scan/scanner_discovery_runner.h"
#include "chrome/browser/extensions/api/document_scan/start_scan_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace extensions {

namespace {

// Error messages that can be included in a response when scanning fails.
constexpr char kNoScannersAvailableError[] = "No scanners available";
constexpr char kUnsupportedMimeTypesError[] = "Unsupported MIME types";
constexpr char kScanImageError[] = "Failed to scan image";
constexpr char kVirtualPrinterUnavailableError[] =
    "Virtual USB printer unavailable";

// The name of the virtual USB printer used for testing.
constexpr char kVirtualUSBPrinter[] = "DavieV Virtual USB Printer (USB)";

// The testing MIME type.
constexpr char kTestingMimeType[] = "testing";

// The PNG MIME type.
constexpr char kScannerImageMimeTypePng[] = "image/png";

// The PNG image data URL prefix of a scanned image.
constexpr char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

crosapi::mojom::DocumentScan* GetDocumentScanInterface() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CrosapiManager is not always initialized in tests.
  if (!crosapi::CrosapiManager::IsInitialized()) {
    CHECK_IS_TEST();
    return nullptr;
  }
  return crosapi::CrosapiManager::Get()->crosapi_ash()->document_scan_ash();
#else
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DocumentScan>()) {
    LOG(ERROR) << "DocumentScan service not available";
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::DocumentScan>().get();
#endif
}

}  // namespace

// static
std::unique_ptr<DocumentScanAPIHandler>
DocumentScanAPIHandler::CreateForTesting(
    content::BrowserContext* browser_context,
    crosapi::mojom::DocumentScan* document_scan) {
  return base::WrapUnique(
      new DocumentScanAPIHandler(browser_context, document_scan));
}

DocumentScanAPIHandler::DocumentScanAPIHandler(
    content::BrowserContext* browser_context)
    : DocumentScanAPIHandler(browser_context, GetDocumentScanInterface()) {}

DocumentScanAPIHandler::DocumentScanAPIHandler(
    content::BrowserContext* browser_context,
    crosapi::mojom::DocumentScan* document_scan)
    : browser_context_(browser_context), document_scan_(document_scan) {
  CHECK(document_scan_);
}

DocumentScanAPIHandler::~DocumentScanAPIHandler() = default;

DocumentScanAPIHandler::ExtensionState::ExtensionState() = default;
DocumentScanAPIHandler::ExtensionState::~ExtensionState() = default;

// static
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>*
DocumentScanAPIHandler::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>>
      instance;
  return instance.get();
}

// static
DocumentScanAPIHandler* DocumentScanAPIHandler::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::Get(
      browser_context);
}

// static
void DocumentScanAPIHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDocumentScanAPITrustedExtensions);
}

void DocumentScanAPIHandler::SetDocumentScanForTesting(
    crosapi::mojom::DocumentScan* document_scan) {
  document_scan_ = document_scan;
}

void DocumentScanAPIHandler::SimpleScan(
    const std::vector<std::string>& mime_types,
    SimpleScanCallback callback) {
  bool should_use_virtual_usb_printer = false;
  if (base::Contains(mime_types, kTestingMimeType)) {
    should_use_virtual_usb_printer = true;
  } else if (!base::Contains(mime_types, kScannerImageMimeTypePng)) {
    std::move(callback).Run(std::nullopt, kUnsupportedMimeTypesError);
    return;
  }

  document_scan_->GetScannerNames(
      base::BindOnce(&DocumentScanAPIHandler::OnSimpleScanNamesReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     should_use_virtual_usb_printer, std::move(callback)));
}

void DocumentScanAPIHandler::OnSimpleScanNamesReceived(
    bool force_virtual_usb_printer,
    SimpleScanCallback callback,
    const std::vector<std::string>& scanner_names) {
  if (scanner_names.empty()) {
    std::move(callback).Run(std::nullopt, kNoScannersAvailableError);
    return;
  }

  // TODO(pstew): Call a delegate method here to select a scanner and options.
  // The first scanner supporting one of the requested MIME types used to be
  // selected. The testing MIME type dictates that the virtual USB printer
  // should be used if available. Otherwise, since all of the scanners always
  // support PNG, select the first scanner in the list.

  std::string scanner_name;
  if (force_virtual_usb_printer) {
    if (!base::Contains(scanner_names, kVirtualUSBPrinter)) {
      std::move(callback).Run(std::nullopt, kVirtualPrinterUnavailableError);
      return;
    }

    scanner_name = kVirtualUSBPrinter;
  } else {
    scanner_name = scanner_names[0];
  }

  document_scan_->ScanFirstPage(
      scanner_name,
      base::BindOnce(&DocumentScanAPIHandler::OnSimpleScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnSimpleScanCompleted(
    SimpleScanCallback callback,
    crosapi::mojom::ScanFailureMode failure_mode,
    const std::optional<std::string>& scan_data) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI and
  // confirm that this scan should be sent to the caller. If this is a
  // multi-page scan, provide a means for adding additional scanned images up to
  // the requested limit.
  if (!scan_data.has_value() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    std::move(callback).Run(std::nullopt, kScanImageError);
    return;
  }

  std::string image_base64 = base::Base64Encode(scan_data.value());
  api::document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix +
                                   std::move(image_base64));
  scan_results.mime_type = kScannerImageMimeTypePng;

  std::move(callback).Run(std::move(scan_results), std::nullopt);
}

void DocumentScanAPIHandler::GetScannerList(
    gfx::NativeWindow native_window,
    scoped_refptr<const Extension> extension,
    api::document_scan::DeviceFilter filter,
    GetScannerListCallback callback) {
  // Invalidate any previously returned scannerId values because the underlying
  // SANE entries aren't stable across multiple calls to sane_get_devices.
  // Removed scannerIds don't need to be explicitly closed because there's no
  // state associated with them on the backend.
  // TODO(b/311196232): Once deviceUuid calculation is stable on the backend,
  // don't erase the whole list.  Instead, preserve entries that point to the
  // same connection string and deviceUuid so that previously-issued tokens
  // remain valid if they still point to the same device.  This includes the
  // `state.scanner_ids` map and the `scanner_devices_` map.
  for (auto& [id, state] : extension_state_) {
    state.scanner_ids.clear();
    scanner_devices_.clear();
    // Exclusive handles that are already open remain valid even after calling
    // sane_get_devices, so deliberately do not clear them nor the associated
    // job handles.
    // TODO(b/316152239): By not clearing these scanner/job handles, they will
    // grow unbounded.  Find the appropriate time to clean them up.
  }

  auto discovery_runner = std::make_unique<ScannerDiscoveryRunner>(
      native_window, browser_context_, std::move(extension), document_scan_);

  ScannerDiscoveryRunner* raw_runner = discovery_runner.get();
  raw_runner->Start(
      crosapi::mojom::ScannerEnumFilter::From(filter),
      base::BindOnce(&DocumentScanAPIHandler::OnScannerListReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(discovery_runner), std::move(callback)));
}

void DocumentScanAPIHandler::OnScannerListReceived(
    std::unique_ptr<ScannerDiscoveryRunner> runner,
    GetScannerListCallback callback,
    crosapi::mojom::GetScannerListResponsePtr mojo_response) {
  auto api_response =
      std::move(mojo_response).To<api::document_scan::GetScannerListResponse>();

  // Replace the SANE connection strings with unguessable tokens to reduce
  // information leakage about the user's network and specific devices.  Also,
  // keep track of the display name for each scanner.
  ExtensionState& state = extension_state_[runner->extension_id()];
  for (auto& scanner : api_response.scanners) {
    std::string token = base::UnguessableToken::Create().ToString();
    const std::string& scanner_id = scanner.scanner_id;
    scanner_devices_[scanner_id] = {.connection_string = scanner_id,
                                    .name = scanner.name};
    state.scanner_ids[token] = scanner_id;
    scanner.scanner_id = std::move(token);
  }

  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::OpenScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_id,
    OpenScannerCallback callback) {
  const ExtensionState& state = extension_state_[extension->id()];

  // Convert the supplied scanner id to the internal connection string needed by
  // the backend.
  auto id_iter = state.scanner_ids.find(scanner_id);
  if (id_iter == state.scanner_ids.end() ||
      !base::Contains(scanner_devices_, id_iter->second)) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnOpenScannerResponse(extension->id(), scanner_id, std::move(callback),
                          std::move(response));
    return;
  }

  document_scan_->OpenScanner(
      extension->id(), scanner_devices_[id_iter->second].connection_string,
      base::BindOnce(&DocumentScanAPIHandler::OnOpenScannerResponse,
                     weak_ptr_factory_.GetWeakPtr(), extension->id(),
                     scanner_id, std::move(callback)));
}

void DocumentScanAPIHandler::OnOpenScannerResponse(
    const ExtensionId& extension_id,
    const std::string& scanner_id,
    OpenScannerCallback callback,
    crosapi::mojom::OpenScannerResponsePtr response) {
  auto response_out = response.To<api::document_scan::OpenScannerResponse>();

  // Replace the internal connection string with the originally requested token.
  response_out.scanner_id = scanner_id;

  if (response_out.result != api::document_scan::OperationResult::kSuccess) {
    std::move(callback).Run(std::move(response_out));
    return;
  }

  ExtensionState& state = extension_state_[extension_id];
  if (!base::Contains(state.scanner_ids, scanner_id)) {
    response_out.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(response_out));
    return;
  }

  // Track that this handle belongs to this extension.  This prevents other
  // extensions from using it.
  if (response_out.scanner_handle.has_value()) {
    state.scanner_handles[response_out.scanner_handle.value()] =
        state.scanner_ids[scanner_id];
  }

  std::move(callback).Run(std::move(response_out));
}

void DocumentScanAPIHandler::GetOptionGroups(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    GetOptionGroupsCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!base::Contains(state.scanner_handles, scanner_handle)) {
    auto response = crosapi::mojom::GetOptionGroupsResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnGetOptionGroupsResponse(std::move(callback), std::move(response));
    return;
  }

  document_scan_->GetOptionGroups(
      scanner_handle,
      base::BindOnce(&DocumentScanAPIHandler::OnGetOptionGroupsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnGetOptionGroupsResponse(
    GetOptionGroupsCallback callback,
    crosapi::mojom::GetOptionGroupsResponsePtr response) {
  std::move(callback).Run(
      response.To<api::document_scan::GetOptionGroupsResponse>());
}

void DocumentScanAPIHandler::CloseScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    CloseScannerCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!base::Contains(state.scanner_handles, scanner_handle)) {
    auto response = crosapi::mojom::CloseScannerResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnCloseScannerResponse(std::move(callback), std::move(response));
    return;
  }

  document_scan_->CloseScanner(
      scanner_handle,
      base::BindOnce(&DocumentScanAPIHandler::OnCloseScannerResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnCloseScannerResponse(
    CloseScannerCallback callback,
    crosapi::mojom::CloseScannerResponsePtr response) {
  std::move(callback).Run(
      response.To<api::document_scan::CloseScannerResponse>());
}

void DocumentScanAPIHandler::SetOptions(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    const std::vector<api::document_scan::OptionSetting>& options_in,
    SetOptionsCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!base::Contains(state.scanner_handles, scanner_handle)) {
    auto response = crosapi::mojom::SetOptionsResponse::New();
    response->scanner_handle = scanner_handle;
    for (const auto& option : options_in) {
      auto result = crosapi::mojom::SetOptionResult::New();
      result->name = option.name;
      result->result = crosapi::mojom::ScannerOperationResult::kInvalid;
      response->results.emplace_back(std::move(result));
    }
    OnSetOptionsResponse(std::move(callback), std::move(response));
    return;
  }

  std::vector<crosapi::mojom::OptionSettingPtr> options_out;
  options_out.reserve(options_in.size());
  for (const auto& option_in : options_in) {
    auto& option_out = options_out.emplace_back(
        crosapi::mojom::OptionSetting::From(option_in));
    if (option_out->value.is_null()) {
      // `option_out` has no value, so no re-mapping is needed.
      continue;
    }

    // `option_out` has valid field values, but value might not match type.  No
    // need to check for most mismatches here because they will be rejected by
    // the backend.
    //
    // However, even if the caller passed syntactically valid numeric values in
    // Javascript, the result that arrives here can contain inconsistencies in
    // double vs integer.  These can happen due to the inherent JS use of double
    // for integers as well as quirks of how the auto-generated IDL mapping code
    // decides to parse arrays for types that accept multiple list types.
    //
    // Detect these specific cases and move the value into the expected fixed or
    // int field before passing along.  All other types are assumed to be
    // supplied correctly by the caller if they have made it through the JS
    // bindings.
    if (option_out->type == crosapi::mojom::OptionType::kFixed) {
      // kFixed is the name for SANE non-integral numeric values.  It is
      // represented in Chrome by double.  Handle getting a long or a list of
      // longs instead of the expected doubles.  This can happen because JS
      // doesn't really have integers, so the framework maps nn.0 into nn.  If
      // this has happened, move the int field over into the expected fixed
      // field.
      if (option_out->value->is_int_value()) {
        option_out->value = crosapi::mojom::OptionValue::NewFixedValue(
            option_out->value->get_int_value());
      } else if (option_out->value->is_int_list()) {
        option_out->value = crosapi::mojom::OptionValue::NewFixedList(
            {option_out->value->get_int_list().begin(),
             option_out->value->get_int_list().end()});
      }
    } else if (option_out->type == crosapi::mojom::OptionType::kInt) {
      // Handle getting a double or a list of doubles instead of the expected
      // int(s).  If the values have zero fractional parts, assume they were
      // really integers that got incorrectly mapped over from JS.  If they have
      // non-zero fractional parts, the caller really passed a double and the
      // value should not be re-mapped.

      auto int_from_double = [](double fixed_value) -> std::optional<int32_t> {
        double int_part = 0.0;
        if (fixed_value >= std::numeric_limits<int32_t>::min() &&
            fixed_value <= std::numeric_limits<int32_t>::max() &&
            std::modf(fixed_value, &int_part) == 0.0) {
          return base::checked_cast<int32_t>(fixed_value);
        }
        return std::nullopt;
      };

      if (option_out->value->is_fixed_value()) {
        auto converted = int_from_double(option_out->value->get_fixed_value());
        if (converted) {
          option_out->value =
              crosapi::mojom::OptionValue::NewIntValue(*converted);
        }
      } else if (option_out->value->is_fixed_list()) {
        std::vector<int32_t> ints;
        const auto& fixed_list = option_out->value->get_fixed_list();
        ints.reserve(fixed_list.size());
        for (const double d : fixed_list) {
          auto converted = int_from_double(d);
          if (!converted) {
            break;  // As soon as there's one non-int, no need to continue.
          }
          ints.push_back(*converted);
        }
        if (ints.size() == fixed_list.size()) {
          option_out->value = crosapi::mojom::OptionValue::NewIntList(
              {ints.begin(), ints.end()});
        }
      }
    }
  }
  document_scan_->SetOptions(
      scanner_handle, std::move(options_out),
      base::BindOnce(&DocumentScanAPIHandler::OnSetOptionsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnSetOptionsResponse(
    SetOptionsCallback callback,
    crosapi::mojom::SetOptionsResponsePtr response) {
  std::move(callback).Run(
      response.To<api::document_scan::SetOptionsResponse>());
}

void DocumentScanAPIHandler::StartScan(
    gfx::NativeWindow native_window,
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    api::document_scan::StartScanOptions options,
    StartScanCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  auto handle_it = state.scanner_handles.find(scanner_handle);
  if (handle_it == state.scanner_handles.end() ||
      !base::Contains(scanner_devices_, handle_it->second)) {
    auto response = crosapi::mojom::StartPreparedScanResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnStartScanResponse(/*runner=*/nullptr, std::move(callback),
                        std::move(response));
    return;
  }

  auto start_runner = std::make_unique<StartScanRunner>(
      native_window, browser_context_, std::move(extension), document_scan_);

  StartScanRunner* raw_runner = start_runner.get();
  raw_runner->Start(
      state.approved_scanners.contains(scanner_handle),
      scanner_devices_[handle_it->second].name, scanner_handle,
      crosapi::mojom::StartScanOptions::From(options),
      base::BindOnce(&DocumentScanAPIHandler::OnStartScanResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(start_runner),
                     std::move(callback)));
}

void DocumentScanAPIHandler::OnStartScanResponse(
    std::unique_ptr<StartScanRunner> runner,
    StartScanCallback callback,
    crosapi::mojom::StartPreparedScanResponsePtr response) {
  auto api_response =
      std::move(response).To<api::document_scan::StartScanResponse>();

  if (runner) {
    ExtensionState& state = extension_state_[runner->extension_id()];

    // If this scanner was approved by the user, keep track so it is not
    // prompted for again.
    if (runner->approved()) {
      state.approved_scanners.insert(api_response.scanner_handle);
    }

    // Keep track of active job handles for this extension.
    if (!api_response.job.value_or("").empty()) {
      state.active_job_handles.insert(api_response.job.value());
    }
  }

  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::CancelScan(
    scoped_refptr<const Extension> extension,
    const std::string& job_handle,
    CancelScanCallback callback) {
  // Ensure this job is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.active_job_handles.contains(job_handle)) {
    auto response = crosapi::mojom::CancelScanResponse::New();
    response->job_handle = job_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnCancelScanResponse(extension->id(), std::move(callback),
                         std::move(response));
    return;
  }

  document_scan_->CancelScan(
      job_handle, base::BindOnce(&DocumentScanAPIHandler::OnCancelScanResponse,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 extension->id(), std::move(callback)));
}

void DocumentScanAPIHandler::OnCancelScanResponse(
    const std::string& extension_id,
    CancelScanCallback callback,
    crosapi::mojom::CancelScanResponsePtr response) {
  auto api_response =
      std::move(response).To<api::document_scan::CancelScanResponse>();

  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::ReadScanData(
    scoped_refptr<const Extension> extension,
    const std::string& job_handle,
    ReadScanDataCallback callback) {
  // Ensure this job is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.active_job_handles.contains(job_handle)) {
    auto response = crosapi::mojom::ReadScanDataResponse::New();
    response->job_handle = job_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnReadScanDataResponse(std::move(callback), std::move(response));
    return;
  }

  document_scan_->ReadScanData(
      job_handle,
      base::BindOnce(&DocumentScanAPIHandler::OnReadScanDataResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnReadScanDataResponse(
    ReadScanDataCallback callback,
    crosapi::mojom::ReadScanDataResponsePtr response) {
  std::move(callback).Run(
      response.To<api::document_scan::ReadScanDataResponse>());
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(context);
  // We do not want an instance of DocumentScanAPIHandler on the lock screen.
  if (!profile->IsRegularProfile()) {
    return nullptr;
  }
  return new DocumentScanAPIHandler(context);
}

}  // namespace extensions
