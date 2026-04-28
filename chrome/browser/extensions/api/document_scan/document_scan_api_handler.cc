// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"
#include "chrome/browser/extensions/api/document_scan/scanner_discovery_runner.h"
#include "chrome/browser/extensions/api/document_scan/simple_scan_runner.h"
#include "chrome/browser/extensions/api/document_scan/start_scan_runner.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

void OnGetOptionGroupsResponse(
    const std::string& scanner_handle,
    DocumentScanAPIHandler::GetOptionGroupsCallback callback,
    const std::optional<lorgnette::GetCurrentConfigResponse>& response) {
  api::document_scan::GetOptionGroupsResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteGetCurrentConfigResponse(*response);
  } else {
    api_response.scanner_handle = scanner_handle;
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }
  std::move(callback).Run(std::move(api_response));
}

void OnSetOptionsResponse(
    const std::string& scanner_handle,
    const std::vector<std::string>& option_names,
    const std::vector<std::string>& invalid_option_names,
    DocumentScanAPIHandler::SetOptionsCallback callback,
    const std::optional<lorgnette::SetOptionsResponse>& response) {
  api::document_scan::SetOptionsResponse api_response;
  if (response.has_value()) {
    api_response = api::document_scan::TransformLorgnetteSetOptionsResponse(
        *response, invalid_option_names);
  } else {
    api_response.scanner_handle = scanner_handle;
    for (const std::string& option_name : option_names) {
      api::document_scan::SetOptionResult result;
      result.name = option_name;
      result.result = api::document_scan::OperationResult::kInternalError;
      api_response.results.push_back(std::move(result));
    }
  }
  std::move(callback).Run(std::move(api_response));
}

void OnReadScanDataResponse(
    const std::string& job_handle,
    DocumentScanAPIHandler::ReadScanDataCallback callback,
    const std::optional<lorgnette::ReadScanDataResponse>& response) {
  api::document_scan::ReadScanDataResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteReadScanDataResponse(*response);
  } else {
    api_response.job = job_handle;
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }

  std::move(callback).Run(std::move(api_response));
}

void OnCancelScanResponse(
    const std::string& job_handle,
    DocumentScanAPIHandler::CancelScanCallback callback,
    const std::optional<lorgnette::CancelScanResponse>& response) {
  api::document_scan::CancelScanResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteCancelScanResponse(*response);
  } else {
    api_response.job = job_handle;
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }
  std::move(callback).Run(std::move(api_response));
}

}  // namespace

DocumentScanAPIHandler::DocumentScanAPIHandler(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
}

DocumentScanAPIHandler::~DocumentScanAPIHandler() = default;

DocumentScanAPIHandler::ExtensionState::ExtensionState()
    : discovery_approved(false) {}
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

void DocumentScanAPIHandler::ExtensionCleanup(const ExtensionId& id) {
  const ExtensionState& state = extension_state_[id];
  for (const auto& [scanner_handle, scanner_id] : state.scanner_handles) {
    // No need to monitor the responses from the CloseScanner call since there
    // is no client waiting for these responses.
    lorgnette::CloseScannerRequest request;
    request.mutable_scanner()->set_token(scanner_handle);
    ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
        ->CloseScanner(request, base::DoNothing());
  }
  extension_state_.erase(id);
}

void DocumentScanAPIHandler::Shutdown() {
  while (!extension_state_.empty()) {
    // `ExtensionCleanup` will remove the given item from the map, so this loop
    // will eventually terminate.
    ExtensionCleanup(extension_state_.begin()->first);
  }
}

void DocumentScanAPIHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  ExtensionCleanup(extension->id());
}

void DocumentScanAPIHandler::SimpleScan(
    scoped_refptr<const Extension> extension,
    const std::vector<std::string>& mime_types,
    SimpleScanCallback callback) {
  auto runner = std::make_unique<SimpleScanRunner>(browser_context_,
                                                   std::move(extension));
  SimpleScanRunner* raw_runner = runner.get();
  raw_runner->Start(
      std::move(mime_types),
      base::BindOnce(&DocumentScanAPIHandler::OnSimpleScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(runner),
                     std::move(callback)));
}

void DocumentScanAPIHandler::OnSimpleScanCompleted(
    std::unique_ptr<SimpleScanRunner> runner,
    SimpleScanCallback callback,
    std::optional<api::document_scan::ScanResults> scan_results,
    std::optional<std::string> error) {
  std::move(callback).Run(std::move(scan_results), std::move(error));
}

void DocumentScanAPIHandler::GetScannerList(
    gfx::NativeWindow native_window,
    scoped_refptr<const Extension> extension,
    bool user_gesture,
    api::document_scan::DeviceFilter filter,
    GetScannerListCallback callback) {
  ExtensionState& state = extension_state_[extension->id()];
  bool approved = state.discovery_approved && user_gesture;

  auto discovery_runner = std::make_unique<ScannerDiscoveryRunner>(
      native_window, browser_context_, std::move(extension));

  ScannerDiscoveryRunner* raw_runner = discovery_runner.get();
  raw_runner->Start(
      approved, std::move(filter),
      base::BindOnce(&DocumentScanAPIHandler::OnScannerListReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(discovery_runner), std::move(callback)));
}

void DocumentScanAPIHandler::OnScannerListReceived(
    std::unique_ptr<ScannerDiscoveryRunner> runner,
    GetScannerListCallback callback,
    api::document_scan::GetScannerListResponse response) {
  // Clear all the previously valid tokens and handles.  The backend has closed
  // any open handles and canceled any active jobs when this extension called
  // GetScannerList.
  ExtensionState& state = extension_state_[runner->extension_id()];
  state.active_scanner_ids.clear();
  state.scanner_handles.clear();
  state.active_job_handles.clear();
  state.approved_scanner_handles.clear();

  // If the response contains any result other than access denied, the user must
  // have approved discovery.  If the result is access denied, the user either
  // denied the discovery dialog or the backend refused to do discovery.  Treat
  // both cases as not approved so the user will be prompted again.
  state.discovery_approved =
      response.result != api::document_scan::OperationResult::kAccessDenied;

  for (auto& scanner : response.scanners) {
    state.active_scanner_ids[scanner.scanner_id] = {.name = scanner.name};
  }

  std::move(callback).Run(std::move(response));
}

void DocumentScanAPIHandler::OpenScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_id,
    OpenScannerCallback callback) {
  const ExtensionState& state = extension_state_[extension->id()];
  if (!state.active_scanner_ids.contains(scanner_id)) {
    lorgnette::OpenScannerResponse response;
    response.mutable_scanner_id()->set_connection_string(scanner_id);
    response.set_result(lorgnette::OPERATION_RESULT_INVALID);
    OnOpenScannerResponse(extension->id(), scanner_id, std::move(callback),
                          std::move(response));
    return;
  }

  lorgnette::OpenScannerRequest request;
  request.mutable_scanner_id()->set_connection_string(scanner_id);
  request.set_client_id(extension->id());
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->OpenScanner(
          request,
          base::BindOnce(&DocumentScanAPIHandler::OnOpenScannerResponse,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         scanner_id, std::move(callback)));
}

void DocumentScanAPIHandler::OnOpenScannerResponse(
    const ExtensionId& extension_id,
    const std::string& scanner_id,
    OpenScannerCallback callback,
    const std::optional<lorgnette::OpenScannerResponse>& response) {
  api::document_scan::OpenScannerResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteOpenScannerResponse(*response);
  } else {
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }

  // Replace the connection string with the originally requested token.
  // TODO(crbug.com/479031241): For other operations we don't override the
  // response like this. We should be consistent.
  api_response.scanner_id = scanner_id;

  if (api_response.result != api::document_scan::OperationResult::kSuccess) {
    std::move(callback).Run(std::move(api_response));
    return;
  }

  ExtensionState& state = extension_state_[extension_id];
  if (!state.active_scanner_ids.contains(scanner_id)) {
    api_response.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(api_response));
    return;
  }

  // Clear any open handles that point to the same scanner.  These are no longer
  // valid after opening a new handle.
  for (auto it = state.scanner_handles.begin();
       it != state.scanner_handles.end();) {
    if (it->second == scanner_id) {
      std::string old_handle = it->first;
      // Erase job handles pointing to the same scanner handle (`old_handle`)
      // before erasing it.
      std::erase_if(state.active_job_handles, [&old_handle](const auto& item) {
        return item.second == old_handle;
      });
      state.approved_scanner_handles.erase(old_handle);
      it = state.scanner_handles.erase(it);
    } else {
      ++it;
    }
  }

  // Track that this handle belongs to this extension.  This prevents other
  // extensions from using it.
  if (api_response.scanner_handle.has_value()) {
    state.scanner_handles[*api_response.scanner_handle] = scanner_id;
  }

  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::GetOptionGroups(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    GetOptionGroupsCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.scanner_handles.contains(scanner_handle)) {
    api::document_scan::GetOptionGroupsResponse response;
    response.scanner_handle = scanner_handle;
    response.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::GetCurrentConfigRequest request;
  request.mutable_scanner()->set_token(scanner_handle);

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->GetCurrentConfig(
          request, base::BindOnce(&OnGetOptionGroupsResponse, scanner_handle,
                                  std::move(callback)));
}

void DocumentScanAPIHandler::CloseScanner(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    CloseScannerCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.scanner_handles.contains(scanner_handle)) {
    api::document_scan::CloseScannerResponse response;
    response.scanner_handle = scanner_handle;
    response.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::CloseScannerRequest request;
  request.mutable_scanner()->set_token(scanner_handle);
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->CloseScanner(
          request,
          base::BindOnce(&DocumentScanAPIHandler::OnCloseScannerResponse,
                         weak_ptr_factory_.GetWeakPtr(), extension->id(),
                         scanner_handle, std::move(callback)));
}

void DocumentScanAPIHandler::OnCloseScannerResponse(
    const ExtensionId& extension_id,
    const std::string& scanner_handle,
    CloseScannerCallback callback,
    const std::optional<lorgnette::CloseScannerResponse>& response) {
  // Stop tracking the handle and remove any job handles pointing to the same
  // scanner handle.
  ExtensionState& state = extension_state_[extension_id];
  std::erase_if(state.active_job_handles, [&scanner_handle](const auto& item) {
    return item.second == scanner_handle;
  });
  state.scanner_handles.erase(scanner_handle);
  state.approved_scanner_handles.erase(scanner_handle);

  api::document_scan::CloseScannerResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteCloseScannerResponse(*response);
  } else {
    api_response.scanner_handle = scanner_handle;
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }
  std::move(callback).Run(std::move(api_response));
}

void DocumentScanAPIHandler::SetOptions(
    scoped_refptr<const Extension> extension,
    const std::string& scanner_handle,
    const std::vector<api::document_scan::OptionSetting>& options_in,
    SetOptionsCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.scanner_handles.contains(scanner_handle)) {
    api::document_scan::SetOptionsResponse response;
    response.scanner_handle = scanner_handle;
    for (const auto& option : options_in) {
      api::document_scan::SetOptionResult result;
      result.name = option.name;
      result.result = api::document_scan::OperationResult::kInvalid;
      response.results.push_back(std::move(result));
    }
    std::move(callback).Run(std::move(response));
    return;
  }

  // Keep track of all of the option names. This is used if we don't get a
  // valid response from the backend. All of these options will get sent back
  // to the caller with an error result.
  std::vector<std::string> option_names;

  // Separately, keep track of any invalid options names (where the type
  // specified for the value does not equal the type of the option).  These
  // options will get sent back to the caller with an appropriate error result.
  std::vector<std::string> invalid_option_names;

  lorgnette::SetOptionsRequest request;
  request.mutable_scanner()->set_token(scanner_handle);
  for (const auto& option_in : options_in) {
    option_names.push_back(option_in.name);
    std::optional<lorgnette::ScannerOption> option =
        api::document_scan::TransformOptionSettingToLorgnetteScannerOption(
            option_in);
    if (option.has_value()) {
      *request.add_options() = std::move(*option);
    } else {
      invalid_option_names.push_back(option_in.name);
    }
  }

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->SetOptions(
          request,
          base::BindOnce(&OnSetOptionsResponse, scanner_handle,
                         std::move(option_names),
                         std::move(invalid_option_names), std::move(callback)));
}

void DocumentScanAPIHandler::StartScan(
    gfx::NativeWindow native_window,
    scoped_refptr<const Extension> extension,
    bool user_gesture,
    const std::string& scanner_handle,
    api::document_scan::StartScanOptions options,
    StartScanCallback callback) {
  // Ensure this scanner is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  auto handle_it = state.scanner_handles.find(scanner_handle);
  if (handle_it == state.scanner_handles.end() ||
      !state.active_scanner_ids.contains(handle_it->second)) {
    lorgnette::StartPreparedScanResponse response;
    response.mutable_scanner()->set_token(scanner_handle);
    response.set_result(lorgnette::OPERATION_RESULT_INVALID);
    OnStartScanResponse(scanner_handle, /*runner=*/nullptr, std::move(callback),
                        response);
    return;
  }

  auto start_runner = std::make_unique<StartScanRunner>(
      native_window, browser_context_, std::move(extension));

  bool approved = state.approved_scanner_handles.contains(scanner_handle) ||
                  (user_gesture && state.approved_scanner_ids.contains(
                                       state.scanner_handles[scanner_handle]));
  StartScanRunner* raw_runner = start_runner.get();
  raw_runner->Start(
      approved, state.active_scanner_ids[handle_it->second].name,
      scanner_handle, std::move(options),
      base::BindOnce(&DocumentScanAPIHandler::OnStartScanResponse,
                     weak_ptr_factory_.GetWeakPtr(), scanner_handle,
                     std::move(start_runner), std::move(callback)));
}

void DocumentScanAPIHandler::OnStartScanResponse(
    const std::string& scanner_handle,
    std::unique_ptr<StartScanRunner> runner,
    StartScanCallback callback,
    const std::optional<lorgnette::StartPreparedScanResponse>& response) {
  api::document_scan::StartScanResponse api_response;
  if (response.has_value()) {
    api_response =
        api::document_scan::ConvertLorgnetteStartPreparedScanResponse(
            *response);
  } else {
    api_response.scanner_handle = scanner_handle;
    api_response.result = api::document_scan::OperationResult::kInternalError;
  }

  if (runner) {
    ExtensionState& state = extension_state_[runner->extension_id()];

    // If this scanner was approved by the user, keep track so it is not
    // prompted for again.
    if (runner->approved()) {
      const std::string& handle = api_response.scanner_handle;
      state.approved_scanner_handles.insert(handle);
      state.approved_scanner_ids.insert(state.scanner_handles[handle]);
    }

    // Keep track of active job handles for this extension.
    if (!api_response.job.value_or("").empty()) {
      state.active_job_handles[api_response.job.value()] =
          api_response.scanner_handle;
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
    api::document_scan::CancelScanResponse response;
    response.job = job_handle;
    response.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::CancelScanRequest request;
  request.mutable_job_handle()->set_token(job_handle);
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->CancelScan(request, base::BindOnce(&OnCancelScanResponse, job_handle,
                                           std::move(callback)));
}

void DocumentScanAPIHandler::ReadScanData(
    scoped_refptr<const Extension> extension,
    const std::string& job_handle,
    ReadScanDataCallback callback) {
  // Ensure this job is allocated to this extension.
  ExtensionState& state = extension_state_[extension->id()];
  if (!state.active_job_handles.contains(job_handle)) {
    api::document_scan::ReadScanDataResponse response;
    response.job = job_handle;
    response.result = api::document_scan::OperationResult::kInvalid;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::ReadScanDataRequest request;
  request.mutable_job_handle()->set_token(job_handle);
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->ReadScanData(request, base::BindOnce(&OnReadScanDataResponse,
                                             job_handle, std::move(callback)));
}

template <>
std::unique_ptr<KeyedService>
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return std::make_unique<DocumentScanAPIHandler>(context);
}

}  // namespace extensions
