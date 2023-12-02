// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"
#include "chrome/browser/extensions/api/document_scan/scanner_discovery_runner.h"
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
    std::move(callback).Run(absl::nullopt, kUnsupportedMimeTypesError);
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
    std::move(callback).Run(absl::nullopt, kNoScannersAvailableError);
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
      std::move(callback).Run(absl::nullopt, kVirtualPrinterUnavailableError);
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
    const absl::optional<std::string>& scan_data) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI and
  // confirm that this scan should be sent to the caller. If this is a
  // multi-page scan, provide a means for adding additional scanned images up to
  // the requested limit.
  if (!scan_data.has_value() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    std::move(callback).Run(absl::nullopt, kScanImageError);
    return;
  }

  std::string image_base64;
  base::Base64Encode(scan_data.value(), &image_base64);
  api::document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix +
                                   std::move(image_base64));
  scan_results.mime_type = kScannerImageMimeTypePng;

  std::move(callback).Run(std::move(scan_results), absl::nullopt);
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
  // remain valid if they still point to the same device.
  for (auto& [id, state] : extension_state_) {
    state.scanner_ids.clear();
    // Exclusive handles that are already open remain valid even after calling
    // sane_get_devices, so deliberately do not clear them.
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
  // information leakage about the user's network and specific devices.
  ExtensionState& state = extension_state_[runner->extension_id()];
  for (auto& scanner : api_response.scanners) {
    std::string token = base::UnguessableToken::Create().ToString();
    state.scanner_ids[token] = scanner.scanner_id;
    scanner.scanner_id = std::move(token);
  }

  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::OpenScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_id,
    OpenScannerCallback callback) {
  // If this extension doesn't have saved state, it must be calling openScanner
  // without previously calling getScannerList.  This means any scanner ID it
  // supplies is invalid.
  if (!base::Contains(extension_state_, extension->id())) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnOpenScannerResponse(extension->id(), scanner_id, std::move(callback),
                          std::move(response));
    return;
  }
  const ExtensionState& state = extension_state_.at(extension->id());

  // Convert the supplied scanner id to the internal connection string needed by
  // the backend.
  if (!base::Contains(state.scanner_ids, scanner_id)) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnOpenScannerResponse(extension->id(), scanner_id, std::move(callback),
                          std::move(response));
    return;
  }
  const std::string& connection_string = state.scanner_ids.at(scanner_id);

  document_scan_->OpenScanner(
      extension->id(), connection_string,
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
  const std::string& connection_string = response_out.scanner_id;
  response_out.scanner_id = scanner_id;

  if (response_out.result != api::document_scan::OperationResult::kSuccess) {
    std::move(callback).Run(std::move(response_out));
    return;
  }

  // Since the call succeeded, the backend has closed any previous handle opened
  // to the same scanner.  Remove these from the list of valid handles.
  auto& open_handles = extension_state_[extension_id].scanner_handles;
  for (auto i = open_handles.begin(); i != open_handles.end();) {
    if (i->second == connection_string) {
      i = open_handles.erase(i);
      continue;
    }
    ++i;
  }

  // Track that this handle belongs to this extension.  This prevents other
  // extensions from using it and allows quick preliminary validity checks
  // without doing an IPC.
  if (response_out.scanner_handle.has_value()) {
    open_handles.try_emplace(response_out.scanner_handle.value(),
                             connection_string);
  }

  std::move(callback).Run(std::move(response_out));
}

bool DocumentScanAPIHandler::IsValidScannerHandle(
    const ExtensionId& extension_id,
    const std::string& scanner_handle) {
  // If this extension doesn't have saved state, it must be trying to use a
  // handle without previously calling openScanner.  This means any scanner
  // handle it supplies is invalid.
  if (!base::Contains(extension_state_, extension_id)) {
    return false;
  }

  // Make sure the scanner handle is an active handle that was previously given
  // to this extension.
  return base::Contains(extension_state_.at(extension_id).scanner_handles,
                        scanner_handle);
}

void DocumentScanAPIHandler::CloseScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    CloseScannerCallback callback) {
  if (!IsValidScannerHandle(extension->id(), scanner_handle)) {
    auto response = crosapi::mojom::CloseScannerResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
    OnCloseScannerResponse(std::move(callback), std::move(response));
    return;
  }

  // Erase the scanner handle even though the response hasn't been received yet.
  // The backend will reject any further calls on a closed handle, so there's no
  // benefit in allowing additional operations to be attempted.
  extension_state_[extension->id()].scanner_handles.erase(scanner_handle);

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
