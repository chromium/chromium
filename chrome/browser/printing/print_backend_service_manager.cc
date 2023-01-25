// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/printing_features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "chrome/browser/printing/printer_xml_parser_impl.h"
#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/backend/win_helper.h"
#include "printing/printed_page_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
#include "base/notreached.h"
#endif

namespace printing {

namespace {

// Histogram name for capturing if any printer drivers were encountered that
// required fallback to workaround an access-denied error.  Determining if this
// happens in the wild would be the impetus to pursue further efforts to
// identify and possibly better rectify such cases.
constexpr char kPrintBackendRequiresElevatedPrivilegeHistogramName[] =
    "Printing.PrintBackend.DriversRequiringElevatedPrivilegeEncountered";

// For fetching remote IDs when there is no printer name.
constexpr char kEmptyPrinterName[] = "";

PrintBackendServiceManager* g_print_backend_service_manager_singleton = nullptr;

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
// TODO(crbug.com/809738):  Update for other platforms as they are made able
// to support modal dialogs from OOP.
uint32_t NativeViewToUint(gfx::NativeView view) {
#if BUILDFLAG(IS_WIN)
  return base::win::HandleToUint32(views::HWNDForNativeView(view));
#else
  NOTREACHED();
  return 0;
#endif
}
#endif

}  // namespace

PrintBackendServiceManager::CallbackContext::CallbackContext() = default;

PrintBackendServiceManager::CallbackContext::CallbackContext(
    PrintBackendServiceManager::CallbackContext&& other) noexcept = default;

PrintBackendServiceManager::CallbackContext::~CallbackContext() = default;

PrintBackendServiceManager::PrintBackendServiceManager() = default;

PrintBackendServiceManager::~PrintBackendServiceManager() = default;

// static
void PrintBackendServiceManager::LogCallToRemote(
    base::StringPiece name,
    const CallbackContext& context) {
  DVLOG(1) << "Sending " << name << " on remote `" << context.remote_id
           << "`, saved callback ID of " << context.saved_callback_id;
}

// static
void PrintBackendServiceManager::LogCallbackFromRemote(
    base::StringPiece name,
    const CallbackContext& context) {
  DVLOG(1) << name << " completed for remote `" << context.remote_id
           << "` saved callback ID " << context.saved_callback_id;
}

void PrintBackendServiceManager::SetCrashKeys(const std::string& printer_name) {
  if (sandboxed_service_remote_for_test_)
    return;

  // TODO(crbug.com/1227561)  Remove local call for driver info, don't want
  // any residual accesses left into the printer drivers from the browser
  // process.
  base::ScopedAllowBlocking allow_blocking;
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());
  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend->GetPrinterDriverInfo(printer_name));
}

PrintBackendServiceManager::ClientId
PrintBackendServiceManager::RegisterQueryClient() {
  return *RegisterClient(ClientType::kQuery, kEmptyPrinterName);
}

absl::optional<PrintBackendServiceManager::ClientId>
PrintBackendServiceManager::RegisterQueryWithUiClient() {
  return RegisterClient(ClientType::kQueryWithUi, kEmptyPrinterName);
}
PrintBackendServiceManager::ClientId
PrintBackendServiceManager::RegisterPrintDocumentClient(
    const std::string& printer_name) {
  DCHECK_NE(printer_name, kEmptyPrinterName);
  return *RegisterClient(ClientType::kPrintDocument, printer_name);
}

PrintBackendServiceManager::ClientId
PrintBackendServiceManager::RegisterPrintDocumentClientReusingClientRemote(
    ClientId id) {
  DCHECK(query_with_ui_clients_.contains(id));
  return *RegisterClient(ClientType::kPrintDocument,
                         query_with_ui_clients_[id]);
}

void PrintBackendServiceManager::UnregisterClient(ClientId id) {
  // Determine which client type has this ID, and remove it once found.
  absl::optional<ClientType> client_type;
  RemoteId remote_id = GetRemoteIdForPrinterName(kEmptyPrinterName);
  if (query_clients_.erase(id) != 0) {
    client_type = ClientType::kQuery;
  } else if (query_with_ui_clients_.erase(id) != 0) {
    client_type = ClientType::kQueryWithUi;
  } else {
    for (auto& item : print_document_clients_) {
      ClientsSet& clients = item.second;
      if (clients.erase(id) != 0) {
        client_type = ClientType::kPrintDocument;
        remote_id = item.first;
        if (clients.empty())
          print_document_clients_.erase(item);
        break;
      }
    }
  }
  if (!client_type.has_value()) {
    DVLOG(1) << "Unknown client ID " << id
             << ", is client being unregistered multiple times?";
    return;
  }
  VLOG(1) << "Unregistering client with ID " << id
          << " from print backend service.";

  absl::optional<base::TimeDelta> new_timeout =
      DetermineIdleTimeoutUpdateOnUnregisteredClient(client_type.value(),
                                                     remote_id);
  if (new_timeout.has_value())
    UpdateServiceIdleTimeoutByRemoteId(remote_id, new_timeout.value());

#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kReadPrinterCapabilitiesWithXps) &&
      query_clients_.empty()) {
    xml_parser_.reset();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void PrintBackendServiceManager::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(kEmptyPrinterName,
                                               ClientType::kQuery, context);

  SaveCallback(GetRemoteSavedEnumeratePrintersCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  LogCallToRemote("EnumeratePrinters", context);
  service->EnumeratePrinters(
      base::BindOnce(&PrintBackendServiceManager::OnDidEnumeratePrinters,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  CallbackContext context;
  auto& service =
      GetServiceAndCallbackContext(printer_name, ClientType::kQuery, context);

  SaveCallback(GetRemoteSavedFetchCapabilitiesCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("FetchCapabilities", context);
  service->FetchCapabilities(
      printer_name,
      base::BindOnce(&PrintBackendServiceManager::OnDidFetchCapabilities,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(kEmptyPrinterName,
                                               ClientType::kQuery, context);

  SaveCallback(
      GetRemoteSavedGetDefaultPrinterNameCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(callback));

  LogCallToRemote("GetDefaultPrinterName", context);
  service->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendServiceManager::OnDidGetDefaultPrinterName,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  CallbackContext context;
  auto& service =
      GetServiceAndCallbackContext(printer_name, ClientType::kQuery, context);

  SaveCallback(GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(
                   context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("GetPrinterSemanticCapsAndDefaults", context);
  service->GetPrinterSemanticCapsAndDefaults(
      printer_name,
      base::BindOnce(
          &PrintBackendServiceManager::OnDidGetPrinterSemanticCapsAndDefaults,
          base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::UseDefaultSettings(
    const std::string& printer_name,
    mojom::PrintBackendService::UseDefaultSettingsCallback callback) {
  // Even though this call does not require a UI, it is used exclusively as
  // part of preparation for system print.  It is called immediately before a
  // call to `AskDefaultSettings()`.  Since that call requires `kQueryWithUi`,
  // it will behave better to ensure this uses the same type to reuse the same
  // process.
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kQueryWithUi, context);

  SaveCallback(GetRemoteSavedUseDefaultSettingsCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("UseDefaultSettings", context);
  service->UseDefaultSettings(
      base::BindOnce(&PrintBackendServiceManager::OnDidUseDefaultSettings,
                     base::Unretained(this), std::move(context)));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrintBackendServiceManager::AskUserForSettings(
    const std::string& printer_name,
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    bool is_scripted,
    mojom::PrintBackendService::AskUserForSettingsCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kQueryWithUi, context);

  SaveCallback(GetRemoteSavedAskUserForSettingsCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("AskUserForSettings", context);
  service->AskUserForSettings(
      NativeViewToUint(parent_view), max_pages, has_selection, is_scripted,
      base::BindOnce(&PrintBackendServiceManager::OnDidAskUserForSettings,
                     base::Unretained(this), std::move(context)));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrintBackendServiceManager::UpdatePrintSettings(
    const std::string& printer_name,
    base::Value::Dict job_settings,
    mojom::PrintBackendService::UpdatePrintSettingsCallback callback) {
  CallbackContext context;
  auto& service =
      GetServiceAndCallbackContext(printer_name, ClientType::kQuery, context);

  SaveCallback(GetRemoteSavedUpdatePrintSettingsCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("UpdatePrintSettings", context);
  service->UpdatePrintSettings(
      std::move(job_settings),
      base::BindOnce(&PrintBackendServiceManager::OnDidUpdatePrintSettings,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::StartPrinting(
    const std::string& printer_name,
    int document_cookie,
    const std::u16string& document_name,
    mojom::PrintTargetType target_type,
    const PrintSettings& settings,
    mojom::PrintBackendService::StartPrintingCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kPrintDocument, context);

  SaveCallback(GetRemoteSavedStartPrintingCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("StartPrinting", context);
  service->StartPrinting(
      document_cookie, document_name, target_type, settings,
      base::BindOnce(&PrintBackendServiceManager::OnDidStartPrinting,
                     base::Unretained(this), std::move(context)));
}

#if BUILDFLAG(IS_WIN)
void PrintBackendServiceManager::RenderPrintedPage(
    const std::string& printer_name,
    int document_cookie,
    const PrintedPage& page,
    mojom::MetafileDataType page_data_type,
    base::ReadOnlySharedMemoryRegion serialized_page_data,
    mojom::PrintBackendService::RenderPrintedPageCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kPrintDocument, context);

  SaveCallback(GetRemoteSavedRenderPrintedPageCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  // Page numbers are 0-based for the printing context.
  const uint32_t page_index = page.page_number() - 1;

  LogCallToRemote("RenderPrintedPage", context);
  service->RenderPrintedPage(
      document_cookie, page_index, page_data_type,
      std::move(serialized_page_data), page.page_size(),
      page.page_content_rect(), page.shrink_factor(),
      base::BindOnce(&PrintBackendServiceManager::OnDidRenderPrintedPage,
                     base::Unretained(this), std::move(context)));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintBackendServiceManager::RenderPrintedDocument(
    const std::string& printer_name,
    int document_cookie,
    uint32_t page_count,
    mojom::MetafileDataType data_type,
    base::ReadOnlySharedMemoryRegion serialized_data,
    mojom::PrintBackendService::RenderPrintedDocumentCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kPrintDocument, context);

  SaveCallback(
      GetRemoteSavedRenderPrintedDocumentCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("RenderPrintedDocument", context);
  service->RenderPrintedDocument(
      document_cookie, page_count, data_type, std::move(serialized_data),
      base::BindOnce(&PrintBackendServiceManager::OnDidRenderPrintedDocument,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::DocumentDone(
    const std::string& printer_name,
    int document_cookie,
    mojom::PrintBackendService::DocumentDoneCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kPrintDocument, context);

  SaveCallback(GetRemoteSavedDocumentDoneCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("DocumentDone", context);
  service->DocumentDone(
      document_cookie,
      base::BindOnce(&PrintBackendServiceManager::OnDidDocumentDone,
                     base::Unretained(this), std::move(context)));
}

void PrintBackendServiceManager::Cancel(
    const std::string& printer_name,
    int document_cookie,
    mojom::PrintBackendService::CancelCallback callback) {
  CallbackContext context;
  auto& service = GetServiceAndCallbackContext(
      printer_name, ClientType::kPrintDocument, context);

  SaveCallback(GetRemoteSavedCancelCallbacks(context.is_sandboxed),
               context.remote_id, context.saved_callback_id,
               std::move(callback));

  SetCrashKeys(printer_name);

  LogCallToRemote("Cancel", context);
  service->Cancel(document_cookie,
                  base::BindOnce(&PrintBackendServiceManager::OnDidCancel,
                                 base::Unretained(this), std::move(context)));
}

bool PrintBackendServiceManager::PrinterDriverFoundToRequireElevatedPrivilege(
    const std::string& printer_name) const {
  return drivers_requiring_elevated_privilege_.contains(printer_name);
}

void PrintBackendServiceManager::
    SetPrinterDriverFoundToRequireElevatedPrivilege(
        const std::string& printer_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "Destination '" << printer_name
          << "' requires elevated privileges.";
  if (drivers_requiring_elevated_privilege_.emplace(printer_name).second &&
      drivers_requiring_elevated_privilege_.size() == 1) {
    // First time we've detected a problem for any driver.
    base::UmaHistogramBoolean(
        kPrintBackendRequiresElevatedPrivilegeHistogramName, /*sample=*/true);
  }
}

void PrintBackendServiceManager::SetServiceForTesting(
    mojo::Remote<mojom::PrintBackendService>* remote) {
  sandboxed_service_remote_for_test_ = remote;
  sandboxed_service_remote_for_test_->set_disconnect_handler(base::BindOnce(
      &PrintBackendServiceManager::OnRemoteDisconnected, base::Unretained(this),
      /*sandboxed=*/true,
      GetRemoteIdForPrinterName(/*printer_name=*/std::string())));
}

void PrintBackendServiceManager::SetServiceForFallbackTesting(
    mojo::Remote<mojom::PrintBackendService>* remote) {
  unsandboxed_service_remote_for_test_ = remote;
  unsandboxed_service_remote_for_test_->set_disconnect_handler(base::BindOnce(
      &PrintBackendServiceManager::OnRemoteDisconnected, base::Unretained(this),
      /*sandboxed=*/false,
      GetRemoteIdForPrinterName(/*printer_name=*/std::string())));
}

// static
PrintBackendServiceManager& PrintBackendServiceManager::GetInstance() {
  if (!g_print_backend_service_manager_singleton) {
    g_print_backend_service_manager_singleton =
        new PrintBackendServiceManager();
  }
  return *g_print_backend_service_manager_singleton;
}

// static
void PrintBackendServiceManager::ResetForTesting() {
  if (g_print_backend_service_manager_singleton) {
    delete g_print_backend_service_manager_singleton;
    g_print_backend_service_manager_singleton = nullptr;
  }
}

PrintBackendServiceManager::RemoteId
PrintBackendServiceManager::GetRemoteIdForPrinterName(
    const std::string& printer_name) {
#if BUILDFLAG(IS_WIN)
  if (!sandboxed_service_remote_for_test_) {
    // Windows drivers are not thread safe.  Use a process per driver to prevent
    // bad interactions when interfacing to multiple drivers in parallel.
    // https://crbug.com/957242
    auto iter = remote_id_map_.find(printer_name);
    if (iter != remote_id_map_.end()) {
      return iter->second;
    }

    // No remote yet for this printer so make one.  RemoteId is only used within
    // browse process management code, so a simple incrementing sequence is
    // sufficient.
    static uint32_t id_sequence = 0;
    return remote_id_map_.insert({printer_name, RemoteId(++id_sequence)})
        .first->second;
  }
#endif

  // Non-Windows platforms and the testing environment always just use one
  // instance for all printers.
  return RemoteId(1);
}

absl::optional<PrintBackendServiceManager::ClientId>
PrintBackendServiceManager::RegisterClient(
    ClientType client_type,
    absl::variant<std::string, RemoteId> destination) {
  ClientId client_id = ClientId(++last_client_id_);
  RemoteId remote_id =
      absl::holds_alternative<std::string>(destination)
          ? GetRemoteIdForPrinterName(
                /*printer_name=*/absl::get<std::string>(destination))
          : absl::get<RemoteId>(destination);

  VLOG(1) << "Registering a client with ID " << client_id
          << " for print backend service.";
  switch (client_type) {
    case ClientType::kQuery:
      query_clients_.insert(client_id);
      break;
    case ClientType::kQueryWithUi:
#if !BUILDFLAG(IS_LINUX)
      if (!query_with_ui_clients_.empty())
        return absl::nullopt;
#endif
      query_with_ui_clients_.insert({client_id, remote_id});
      break;
    case ClientType::kPrintDocument:
      print_document_clients_[remote_id].insert(client_id);
      break;
  }

  // A new client registration is a signal of impending activity to a print
  // backend service.  Performance can be improved if we ensure that an initial
  // service is ready for when the first Mojo call should happen shortly after
  // this registration.
  // It is possible that there might have been prior clients registered that
  // persisted for a long time (e.g., a tab with a Print Preview left open
  // indefinitely).  We use a long timeout against idleness for that scenario,
  // so we want to perform this optimization check every time regardless of
  // number of clients registered.
  // System print is a special case because it can display a system dialog and
  // is window modal.  In this scenario we do not want the print backend to
  // self-terminate even if the user is idle for a long period of time.
  if (base::Contains(sandboxed_remotes_bundles_, remote_id)) {
    // Service already existed, possibly was recently marked for being reset
    // with a short timeout or is already in use for other client types.
    // Determine if any adjustment to the timeout is actually necessary.
    absl::optional<base::TimeDelta> new_timeout =
        DetermineIdleTimeoutUpdateOnRegisteredClient(client_type, remote_id);
    if (new_timeout.has_value())
      UpdateServiceIdleTimeoutByRemoteId(remote_id, new_timeout.value());
  } else {
    // Service not already available, so launch it now so that it will be
    // ready by the time the client gets to point of invoking a Mojo call.
    DCHECK(absl::holds_alternative<std::string>(destination));
    bool is_sandboxed;
    GetService(/*printer_name=*/absl::get<std::string>(destination),
               client_type, &is_sandboxed);
  }

  return client_id;
}

size_t PrintBackendServiceManager::GetClientsRegisteredCount() const {
  size_t clients_count = query_clients_.size() + query_with_ui_clients_.size();
  for (auto& item : print_document_clients_)
    clients_count += item.second.size();
  return clients_count;
}

#if BUILDFLAG(IS_WIN)
bool PrintBackendServiceManager::PrinterDriverKnownToRequireElevatedPrivilege(
    const std::string& printer_name,
    ClientType client_type) {
  // Any Windows printer driver which causes a UI dialog to be displayed does
  // not work if printing is started from within a sandboxed environment.
  // crbug.com/1243873
  switch (client_type) {
    case ClientType::kQuery:
      return false;
    case ClientType::kQueryWithUi:
      // Guaranteed to display the system print dialog.
      return true;
    case ClientType::kPrintDocument:
      // Drivers with a print port that results in saving to a file will cause
      // a system dialog to be displayed.
      return DoesDriverDisplayFileDialogForPrinting(printer_name);
  }
}
#endif  // BUILDFLAG(IS_WIN)

const mojo::Remote<mojom::PrintBackendService>&
PrintBackendServiceManager::GetService(const std::string& printer_name,
                                       ClientType client_type,
                                       bool* is_sandboxed) {
  // Determine if sandboxing is appropriate.  This might be already known for
  // certain drivers/configurations, or learned during runtime.
  bool should_sandbox =
      features::kEnableOopPrintDriversSandbox.Get() &&
      !PrinterDriverFoundToRequireElevatedPrivilege(printer_name);
#if BUILDFLAG(IS_WIN)
  if (should_sandbox) {
    should_sandbox = !PrinterDriverKnownToRequireElevatedPrivilege(printer_name,
                                                                   client_type);
  }
#endif
  *is_sandboxed = should_sandbox;

  if (sandboxed_service_remote_for_test_) {
    // The presence of a sandboxed remote for testing signals a testing
    // environment.  If no unsandboxed test service was provided for fallback
    // processing then use the sandboxed one for that as well.
    if (!should_sandbox && unsandboxed_service_remote_for_test_)
      return *unsandboxed_service_remote_for_test_;

    return *sandboxed_service_remote_for_test_;
  }

  // Performance is improved if a service is launched ahead of the time it will
  // be needed by client callers.
  DCHECK_GT(GetClientsRegisteredCount(), 0u);

  if (should_sandbox) {
    // On the first print that will try to use sandboxed service, make note that
    // so far no drivers have been discovered to require fallback beyond any
    // predetermined known cases.
    static bool first_sandboxed_print = true;
    if (first_sandboxed_print) {
      first_sandboxed_print = false;
      base::UmaHistogramBoolean(
          kPrintBackendRequiresElevatedPrivilegeHistogramName,
          /*sample=*/false);
    }
  }

  RemoteId remote_id = GetRemoteIdForPrinterName(printer_name);
  if (should_sandbox) {
    return GetServiceFromBundle(remote_id, client_type, /*sandboxed=*/true,
                                sandboxed_remotes_bundles_);
  }
  return GetServiceFromBundle(remote_id, client_type, /*sandboxed=*/false,
                              unsandboxed_remotes_bundles_);
}

template <class T>
mojo::Remote<mojom::PrintBackendService>&
PrintBackendServiceManager::GetServiceFromBundle(
    const RemoteId& remote_id,
    ClientType client_type,
    bool sandboxed,
    RemotesBundleMap<T>& bundle_map) {
  auto iter = bundle_map.find(remote_id);
  if (iter == bundle_map.end()) {
    // First time for this `remote_id`.
    auto result =
        bundle_map.emplace(remote_id, std::make_unique<RemotesBundle<T>>());
    iter = result.first;
  }

  RemotesBundle<T>* bundle = iter->second.get();
  mojo::Remote<mojom::PrintBackendService>& service = bundle->service;
  if (!service) {
    VLOG(1) << "Launching print backend "
            << (sandboxed ? "sandboxed" : "unsandboxed") << " for `"
            << remote_id << "`";

    mojo::Remote<T>& host = bundle->host;
    content::ServiceProcessHost::Launch(
        host.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_PRINT_BACKEND_SERVICE_NAME)
            .Pass());
    host->BindBackend(service.BindNewPipeAndPassReceiver());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) then we will drop our handle to the remote.
    // Safe to use base::Unretained(this) since `this` is a global singleton
    // which never goes away.
    service.set_disconnect_handler(
        base::BindOnce(&PrintBackendServiceManager::OnRemoteDisconnected,
                       base::Unretained(this), sandboxed, remote_id));

    // We may want to have the service terminated when idle.
    SetServiceIdleHandler(service, sandboxed, remote_id,
                          GetClientTypeIdleTimeout(client_type));
#if BUILDFLAG(IS_WIN)
    // Initialize the new service for the desired locale. Bind
    // PrintBackendService with a Remote that allows pass-through requests to an
    // XML parser.
    mojo::PendingRemote<mojom::PrinterXmlParser> remote;
    if (base::FeatureList::IsEnabled(
            features::kReadPrinterCapabilitiesWithXps)) {
      if (!xml_parser_)
        xml_parser_ = std::make_unique<PrinterXmlParserImpl>();
      remote = xml_parser_->GetRemote();
    }
    service->Init(g_browser_process->GetApplicationLocale(), std::move(remote));
#else
    // Initialize the new service for the desired locale.
    service->Init(g_browser_process->GetApplicationLocale());
#endif  // BUILDFLAG(IS_WIN)
  }

  return service;
}

// static
constexpr base::TimeDelta PrintBackendServiceManager::GetClientTypeIdleTimeout(
    ClientType client_type) {
  switch (client_type) {
    case ClientType::kQuery:
      // Want a long timeout so that the service is available across typical
      // user interactions but will get reclaimed should the user leave it
      // unused indefinitely.  E.g., a Print Preview left open in a tab while
      // user has moved on to other tabs.
      return kClientsRegisteredResetOnIdleTimeout;

    case ClientType::kQueryWithUi:
      // Want the service to remain indefinitely, since this case supports a
      // window-modal system dialog being invoked from the service.
      return base::TimeDelta::Max();

    case ClientType::kPrintDocument:
      // Some print jobs can take a very long time to print.  Choosing some
      // threshold for reclaiming is hard to make and still have the effect
      // of service reclamation meaningful.  Err towards not accidentally
      // terminating an in-progress print job and let the service remain open
      // indefinitely, instead relying upon the registered clients mechanism
      // to reinstate a short timeout once a print job has completed.
      // For Windows there is the additional case where the driver might need
      // to display a file save system dialog (e.g., if the driver sends to
      // port FILE:), which is another window-modal system dialog invoked from
      // the service for which we would want to wait indefinitely.
      return base::TimeDelta::Max();
  }
}

bool PrintBackendServiceManager::HasQueryWithUiClientForRemoteId(
    const RemoteId& remote_id) const {
  for (auto& item : query_with_ui_clients_) {
    if (item.second == remote_id) {
      return true;
    }
  }
  return false;
}

bool PrintBackendServiceManager::HasPrintDocumentClientForRemoteId(
    const RemoteId& remote_id) const {
  return GetPrintDocumentClientsCountForRemoteId(remote_id) > 0;
}

size_t PrintBackendServiceManager::GetPrintDocumentClientsCountForRemoteId(
    const RemoteId& remote_id) const {
  auto iter = print_document_clients_.find(remote_id);
  if (iter != print_document_clients_.end()) {
    DCHECK(!iter->second.empty());
    return iter->second.size();
  }
  return 0;
}

absl::optional<base::TimeDelta>
PrintBackendServiceManager::DetermineIdleTimeoutUpdateOnRegisteredClient(
    ClientType registered_client_type,
    const RemoteId& remote_id) const {
  switch (registered_client_type) {
    case ClientType::kQuery:
      DCHECK(!query_clients_.empty());

      // Other query types have longer timeouts, so no need to update if
      // any of them have clients.
      if (!query_with_ui_clients_.empty() ||
          HasPrintDocumentClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }

      if (query_clients_.size() > 1)
        return absl::nullopt;

      // First client of type and no others will need an update.
      break;

    case ClientType::kQueryWithUi:
#if BUILDFLAG(IS_LINUX)
      // No need to update if there were other query with UI clients.
      if (query_with_ui_clients_.size() > 1)
        return absl::nullopt;
#else
      // A modal system dialog, of which there should only ever be at most one
      // of these.
      DCHECK_EQ(query_with_ui_clients_.size(), 1u);
#endif

      // This is the longest timeout.  No need to update if there is a similarly
      // long timeout due to a printing client.
      if (HasPrintDocumentClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }
      break;

    case ClientType::kPrintDocument:
      size_t clients_count = GetPrintDocumentClientsCountForRemoteId(remote_id);
      DCHECK_GT(clients_count, 0u);

      // No need to update if there were other printing clients for same remote
      // ID.
      if (clients_count > 1)
        return absl::nullopt;

      // This is the longest timeout.  No need to update if there is a similarly
      // long timeout due to query with UI.
      if (!query_with_ui_clients_.empty())
        return absl::nullopt;
      break;
  }

  return GetClientTypeIdleTimeout(registered_client_type);
}

absl::optional<base::TimeDelta>
PrintBackendServiceManager::DetermineIdleTimeoutUpdateOnUnregisteredClient(
    ClientType unregistered_client_type,
    const RemoteId& remote_id) const {
  switch (unregistered_client_type) {
    case ClientType::kQuery:
      // Other query types have longer timeouts, so no need to update if
      // any of them have clients.
      if (HasQueryWithUiClientForRemoteId(remote_id) ||
          HasPrintDocumentClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }

      if (!query_clients_.empty())
        return absl::nullopt;

      // No remaining clients, can switch to short timeout for quick
      // reclamation.
      return kNoClientsRegisteredResetOnIdleTimeout;

    case ClientType::kQueryWithUi:
#if BUILDFLAG(IS_LINUX)
      // No need to update if there were other query with UI clients.
      if (HasQueryWithUiClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }
#else
      // A modal system dialog, of which there should only ever be at most one
      // of these. If one was dropped, it should now be empty.
      DCHECK(query_with_ui_clients_.empty());
#endif

      // This is the longest timeout, so no need to update if there is a
      // printing client for this `remote_id`.
      if (HasPrintDocumentClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }

      // New timeout depends upon existence of other queries.
      return query_clients_.empty() ? kNoClientsRegisteredResetOnIdleTimeout
                                    : kClientsRegisteredResetOnIdleTimeout;

    case ClientType::kPrintDocument:
      // No need to update if there were other printing clients or query with
      // UI clients for same remote ID.
      if (HasQueryWithUiClientForRemoteId(remote_id) ||
          HasPrintDocumentClientForRemoteId(remote_id)) {
        return absl::nullopt;
      }

      // New timeout depends upon existence of other queries.
      return query_clients_.empty() ? kNoClientsRegisteredResetOnIdleTimeout
                                    : kClientsRegisteredResetOnIdleTimeout;
  }
}

void PrintBackendServiceManager::SetServiceIdleHandler(
    mojo::Remote<printing::mojom::PrintBackendService>& service,
    bool sandboxed,
    const RemoteId& remote_id,
    const base::TimeDelta& timeout) {
  DVLOG(1) << "Updating idle timeout for "
           << (sandboxed ? "sandboxed" : "unsandboxed")
           << " print backend service id `" << remote_id << "` to " << timeout;
  service.set_idle_handler(
      kNoClientsRegisteredResetOnIdleTimeout,
      base::BindRepeating(&PrintBackendServiceManager::OnIdleTimeout,
                          base::Unretained(this), sandboxed, remote_id));

  // TODO(crbug.com/1225111)  Make a superfluous call to the service, just to
  // cause an IPC that will in turn make the adjusted timeout value actually
  // take effect.
  service->Poke();
}

void PrintBackendServiceManager::UpdateServiceIdleTimeoutByRemoteId(
    const RemoteId& remote_id,
    const base::TimeDelta& timeout) {
  auto sandboxed_iter = sandboxed_remotes_bundles_.find(remote_id);
  if (sandboxed_iter != sandboxed_remotes_bundles_.end()) {
    RemotesBundle<mojom::SandboxedPrintBackendHost>* bundle =
        sandboxed_iter->second.get();
    SetServiceIdleHandler(bundle->service, /*sandboxed=*/true, remote_id,
                          timeout);
  }
  auto unsandboxed_iter = unsandboxed_remotes_bundles_.find(remote_id);
  if (unsandboxed_iter != unsandboxed_remotes_bundles_.end()) {
    RemotesBundle<mojom::UnsandboxedPrintBackendHost>* bundle =
        unsandboxed_iter->second.get();
    SetServiceIdleHandler(bundle->service, /*sandboxed=*/false, remote_id,
                          timeout);
  }
}

void PrintBackendServiceManager::OnIdleTimeout(bool sandboxed,
                                               const RemoteId& remote_id) {
  DVLOG(1) << "Print Backend service idle timeout for "
           << (sandboxed ? "sandboxed" : "unsandboxed") << " remote id `"
           << remote_id << "`";
  if (sandboxed) {
    sandboxed_remotes_bundles_.erase(remote_id);
  } else {
    unsandboxed_remotes_bundles_.erase(remote_id);
  }
}

void PrintBackendServiceManager::OnRemoteDisconnected(
    bool sandboxed,
    const RemoteId& remote_id) {
  DVLOG(1) << "Print Backend service disconnected for "
           << (sandboxed ? "sandboxed" : "unsandboxed") << " remote id `"
           << remote_id << "`";
  if (sandboxed) {
    sandboxed_remotes_bundles_.erase(remote_id);
  } else {
    unsandboxed_remotes_bundles_.erase(remote_id);
  }
  RunSavedCallbacksStructResult(
      GetRemoteSavedEnumeratePrintersCallbacks(sandboxed), remote_id,
      mojom::PrinterListResult::NewResultCode(mojom::ResultCode::kFailed));
  RunSavedCallbacksStructResult(
      GetRemoteSavedFetchCapabilitiesCallbacks(sandboxed), remote_id,
      mojom::PrinterCapsAndInfoResult::NewResultCode(
          mojom::ResultCode::kFailed));
  RunSavedCallbacksStructResult(
      GetRemoteSavedGetDefaultPrinterNameCallbacks(sandboxed), remote_id,
      mojom::DefaultPrinterNameResult::NewResultCode(
          mojom::ResultCode::kFailed));
  RunSavedCallbacksStructResult(
      GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(sandboxed),
      remote_id,
      mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(
          mojom::ResultCode::kFailed));
  RunSavedCallbacksStructResult(
      GetRemoteSavedUseDefaultSettingsCallbacks(sandboxed), remote_id,
      mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  RunSavedCallbacksStructResult(
      GetRemoteSavedAskUserForSettingsCallbacks(sandboxed), remote_id,
      mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
#endif
  RunSavedCallbacksStructResult(
      GetRemoteSavedUpdatePrintSettingsCallbacks(sandboxed), remote_id,
      mojom::PrintSettingsResult::NewResultCode(mojom::ResultCode::kFailed));
  RunSavedCallbacks(GetRemoteSavedStartPrintingCallbacks(sandboxed), remote_id,
                    mojom::ResultCode::kFailed);
#if BUILDFLAG(IS_WIN)
  RunSavedCallbacks(GetRemoteSavedRenderPrintedPageCallbacks(sandboxed),
                    remote_id, mojom::ResultCode::kFailed);
#endif
  RunSavedCallbacks(GetRemoteSavedRenderPrintedDocumentCallbacks(sandboxed),
                    remote_id, mojom::ResultCode::kFailed);
  RunSavedCallbacks(GetRemoteSavedDocumentDoneCallbacks(sandboxed), remote_id,
                    mojom::ResultCode::kFailed);
  RunSavedCallbacks(GetRemoteSavedCancelCallbacks(sandboxed), remote_id);
}

PrintBackendServiceManager::RemoteSavedEnumeratePrintersCallbacks&
PrintBackendServiceManager::GetRemoteSavedEnumeratePrintersCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_enumerate_printers_callbacks_
                   : unsandboxed_saved_enumerate_printers_callbacks_;
}

PrintBackendServiceManager::RemoteSavedFetchCapabilitiesCallbacks&
PrintBackendServiceManager::GetRemoteSavedFetchCapabilitiesCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_fetch_capabilities_callbacks_
                   : unsandboxed_saved_fetch_capabilities_callbacks_;
}

PrintBackendServiceManager::RemoteSavedGetDefaultPrinterNameCallbacks&
PrintBackendServiceManager::GetRemoteSavedGetDefaultPrinterNameCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_get_default_printer_name_callbacks_
                   : unsandboxed_saved_get_default_printer_name_callbacks_;
}

PrintBackendServiceManager::
    RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks&
    PrintBackendServiceManager::
        GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(
            bool sandboxed) {
  return sandboxed
             ? sandboxed_saved_get_printer_semantic_caps_and_defaults_callbacks_
             : unsandboxed_saved_get_printer_semantic_caps_and_defaults_callbacks_;
}

PrintBackendServiceManager::RemoteSavedUseDefaultSettingsCallbacks&
PrintBackendServiceManager::GetRemoteSavedUseDefaultSettingsCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_use_default_settings_callbacks_
                   : unsandboxed_saved_use_default_settings_callbacks_;
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
PrintBackendServiceManager::RemoteSavedAskUserForSettingsCallbacks&
PrintBackendServiceManager::GetRemoteSavedAskUserForSettingsCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_ask_user_for_settings_callbacks_
                   : unsandboxed_saved_ask_user_for_settings_callbacks_;
}
#endif

PrintBackendServiceManager::RemoteSavedUpdatePrintSettingsCallbacks&
PrintBackendServiceManager::GetRemoteSavedUpdatePrintSettingsCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_update_print_settings_callbacks_
                   : unsandboxed_saved_update_print_settings_callbacks_;
}

PrintBackendServiceManager::RemoteSavedStartPrintingCallbacks&
PrintBackendServiceManager::GetRemoteSavedStartPrintingCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_start_printing_callbacks_
                   : unsandboxed_saved_start_printing_callbacks_;
}

#if BUILDFLAG(IS_WIN)
PrintBackendServiceManager::RemoteSavedRenderPrintedPageCallbacks&
PrintBackendServiceManager::GetRemoteSavedRenderPrintedPageCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_render_printed_page_callbacks_
                   : unsandboxed_saved_render_printed_page_callbacks_;
}
#endif

PrintBackendServiceManager::RemoteSavedRenderPrintedDocumentCallbacks&
PrintBackendServiceManager::GetRemoteSavedRenderPrintedDocumentCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_render_printed_document_callbacks_
                   : unsandboxed_saved_render_printed_document_callbacks_;
}

PrintBackendServiceManager::RemoteSavedDocumentDoneCallbacks&
PrintBackendServiceManager::GetRemoteSavedDocumentDoneCallbacks(
    bool sandboxed) {
  return sandboxed ? sandboxed_saved_document_done_callbacks_
                   : unsandboxed_saved_document_done_callbacks_;
}

PrintBackendServiceManager::RemoteSavedCancelCallbacks&
PrintBackendServiceManager::GetRemoteSavedCancelCallbacks(bool sandboxed) {
  return sandboxed ? sandboxed_saved_cancel_callbacks_
                   : unsandboxed_saved_cancel_callbacks_;
}

const mojo::Remote<mojom::PrintBackendService>&
PrintBackendServiceManager::GetServiceAndCallbackContext(
    const std::string& printer_name,
    ClientType client_type,
    CallbackContext& context) {
  context.remote_id = GetRemoteIdForPrinterName(printer_name);
  context.saved_callback_id = base::UnguessableToken::Create();
  return GetService(printer_name, client_type, &context.is_sandboxed);
}

template <class... T, class... X>
void PrintBackendServiceManager::SaveCallback(
    RemoteSavedCallbacks<T...>& saved_callbacks,
    const RemoteId& remote_id,
    const base::UnguessableToken& saved_callback_id,
    base::OnceCallback<void(X...)> callback) {
  saved_callbacks[remote_id].emplace(saved_callback_id, std::move(callback));
}

template <class... T, class... X>
void PrintBackendServiceManager::ServiceCallbackDone(
    RemoteSavedCallbacks<T...>& saved_callbacks,
    const RemoteId& remote_id,
    const base::UnguessableToken& saved_callback_id,
    X... data) {
  auto found_callback_map = saved_callbacks.find(remote_id);
  DCHECK(found_callback_map != saved_callbacks.end());

  SavedCallbacks<T...>& callback_map = found_callback_map->second;

  auto callback_entry = callback_map.find(saved_callback_id);
  DCHECK(callback_entry != callback_map.end());
  base::OnceCallback<void(X...)> callback = std::move(callback_entry->second);
  callback_map.erase(callback_entry);

  // Done disconnect wrapper management, propagate the callback.
  std::move(callback).Run(std::forward<X>(data)...);
}

void PrintBackendServiceManager::OnDidEnumeratePrinters(
    const CallbackContext& context,
    mojom::PrinterListResultPtr printer_list) {
  LogCallbackFromRemote("EnumeratePrinters", context);
  ServiceCallbackDone(
      GetRemoteSavedEnumeratePrintersCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(printer_list));
}

void PrintBackendServiceManager::OnDidFetchCapabilities(
    const CallbackContext& context,
    mojom::PrinterCapsAndInfoResultPtr printer_caps_and_info) {
  LogCallbackFromRemote("FetchCapabilities", context);
  ServiceCallbackDone(
      GetRemoteSavedFetchCapabilitiesCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id,
      std::move(printer_caps_and_info));
}

void PrintBackendServiceManager::OnDidGetDefaultPrinterName(
    const CallbackContext& context,
    mojom::DefaultPrinterNameResultPtr printer_name) {
  LogCallbackFromRemote("GetDefaultPrinterName", context);
  ServiceCallbackDone(
      GetRemoteSavedGetDefaultPrinterNameCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(printer_name));
}

void PrintBackendServiceManager::OnDidGetPrinterSemanticCapsAndDefaults(
    const CallbackContext& context,
    mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps) {
  LogCallbackFromRemote("GetPrinterSemanticCapsAndDefaults", context);
  ServiceCallbackDone(GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(
                          context.is_sandboxed),
                      context.remote_id, context.saved_callback_id,
                      std::move(printer_caps));
}

void PrintBackendServiceManager::OnDidUseDefaultSettings(
    const CallbackContext& context,
    mojom::PrintSettingsResultPtr settings) {
  LogCallbackFromRemote("UseDefaultSettings", context);
  ServiceCallbackDone(
      GetRemoteSavedUseDefaultSettingsCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(settings));
}

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
void PrintBackendServiceManager::OnDidAskUserForSettings(
    const CallbackContext& context,
    mojom::PrintSettingsResultPtr settings) {
  LogCallbackFromRemote("AskUserForSettings", context);
  ServiceCallbackDone(
      GetRemoteSavedAskUserForSettingsCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(settings));
}
#endif  // BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)

void PrintBackendServiceManager::OnDidUpdatePrintSettings(
    const CallbackContext& context,
    mojom::PrintSettingsResultPtr settings) {
  LogCallbackFromRemote("UpdatePrintSettings", context);
  ServiceCallbackDone(
      GetRemoteSavedUpdatePrintSettingsCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, std::move(settings));
}

void PrintBackendServiceManager::OnDidStartPrinting(
    const CallbackContext& context,
    mojom::ResultCode result) {
  LogCallbackFromRemote("StartPrinting", context);
  ServiceCallbackDone(
      GetRemoteSavedStartPrintingCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, result);
}

#if BUILDFLAG(IS_WIN)
void PrintBackendServiceManager::OnDidRenderPrintedPage(
    const CallbackContext& context,
    mojom::ResultCode result) {
  LogCallbackFromRemote("RenderPrintedPage", context);
  ServiceCallbackDone(
      GetRemoteSavedRenderPrintedPageCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, result);
}
#endif

void PrintBackendServiceManager::OnDidRenderPrintedDocument(
    const CallbackContext& context,
    mojom::ResultCode result) {
  LogCallbackFromRemote("RenderPrintedDocument", context);
  ServiceCallbackDone(
      GetRemoteSavedRenderPrintedDocumentCallbacks(context.is_sandboxed),
      context.remote_id, context.saved_callback_id, result);
}

void PrintBackendServiceManager::OnDidDocumentDone(
    const CallbackContext& context,
    mojom::ResultCode result) {
  LogCallbackFromRemote("DocumentDone", context);
  ServiceCallbackDone(GetRemoteSavedDocumentDoneCallbacks(context.is_sandboxed),
                      context.remote_id, context.saved_callback_id, result);
}

void PrintBackendServiceManager::OnDidCancel(const CallbackContext& context) {
  LogCallbackFromRemote("Cancel", context);
  ServiceCallbackDone(GetRemoteSavedCancelCallbacks(context.is_sandboxed),
                      context.remote_id, context.saved_callback_id);
}

template <class T>
void PrintBackendServiceManager::RunSavedCallbacksStructResult(
    RemoteSavedStructCallbacks<T>& saved_callbacks,
    const RemoteId& remote_id,
    mojo::StructPtr<T> result_to_clone) {
  auto found_callbacks_map = saved_callbacks.find(remote_id);
  if (found_callbacks_map == saved_callbacks.end())
    return;  // No callbacks to run.

  SavedCallbacks<mojo::StructPtr<T>>& callbacks_map =
      found_callbacks_map->second;
  for (auto& iter : callbacks_map) {
    const base::UnguessableToken& saved_callback_id = iter.first;
    DVLOG(1) << "Propagating print backend callback, saved callback ID "
             << saved_callback_id << " for remote `" << remote_id << "`";

    // Don't remove entries from the map while we are iterating through it,
    // just run the callbacks.
    base::OnceCallback<void(mojo::StructPtr<T>)>& callback = iter.second;
    std::move(callback).Run(result_to_clone.Clone());
  }

  // Now that we're done iterating we can safely delete all of the callbacks.
  callbacks_map.clear();
}

template <class... T>
void PrintBackendServiceManager::RunSavedCallbacks(
    RemoteSavedCallbacks<T...>& saved_callbacks,
    const RemoteId& remote_id,
    T... result) {
  auto found_callbacks_map = saved_callbacks.find(remote_id);
  if (found_callbacks_map == saved_callbacks.end())
    return;  // No callbacks to run.

  SavedCallbacks<T...>& callbacks_map = found_callbacks_map->second;
  for (auto& iter : callbacks_map) {
    const base::UnguessableToken& saved_callback_id = iter.first;
    DVLOG(1) << "Propagating print backend callback, saved callback ID "
             << saved_callback_id << " for remote `" << remote_id << "`";

    // Don't remove entries from the map while we are iterating through it,
    // just run the callbacks.
    base::OnceCallback<void(T...)>& callback = iter.second;
    std::move(callback).Run(result...);
  }

  // Now that we're done iterating we can safely delete all of the callbacks.
  callbacks_map.clear();
}

// static
void PrintBackendServiceManager::SetClientsForTesting(
    const ClientsSet& query_clients,
    const QueryWithUiClientsMap& query_with_ui_clients,
    const PrintClientsMap& print_document_clients) {
  g_print_backend_service_manager_singleton->query_clients_ = query_clients;
  g_print_backend_service_manager_singleton->query_with_ui_clients_ =
      query_with_ui_clients;
  g_print_backend_service_manager_singleton->print_document_clients_ =
      print_document_clients;
}

}  // namespace printing
