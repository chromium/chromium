// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_manager.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/print_backend.h"

namespace printing {

namespace {

// Histogram name for capturing if any printer drivers were encountered that
// required fallback to workaround an access-denied error.  Determining if this
// happens in the wild would be the impetus to pursue further efforts to
// identify and possibly better rectify such cases.
constexpr char kPrintBackendRequiresElevatedPrivilegeHistogramName[] =
    "Printing.PrintBackend.DriversRequiringElevatedPrivilegeEncountered";

// Amount of idle time to wait before resetting the connection to the service.
constexpr base::TimeDelta kNoClientsRegisteredResetOnIdleTimeout =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kClientsRegisteredResetOnIdleTimeout =
    base::TimeDelta::FromSeconds(120);

PrintBackendServiceManager* g_print_backend_service_manager_singleton = nullptr;

}  // namespace

PrintBackendServiceManager::PrintBackendServiceManager() = default;

PrintBackendServiceManager::~PrintBackendServiceManager() = default;

bool PrintBackendServiceManager::ShouldSandboxPrintBackendService() const {
  return is_sandboxed_service_;
}

uint32_t PrintBackendServiceManager::RegisterClient() {
  uint32_t client_id = ++last_client_id_;

  VLOG(1) << "Registering a client with ID " << client_id
          << " for print backend service.";
  clients_.emplace(client_id);

  // A new client registration is a signal of impending activity to a print
  // backend service.  Performance can be improved if we ensure that an initial
  // service is ready for when the first Mojo call should happen shortly after
  // this registration.
  // It is possible that there might have been prior clients registered that
  // persisted for a long time (e.g., a tab with a Print Preview left open
  // indefinitely).  We use a long timeout against idleness for that scenario,
  // so we want to perform this optimization check every time regardless of
  // number of clients registered.
  // We don't know if a particular printer might be needed, so for now just
  // start for the blank `printer_name` which would cover queries like getting
  // the default printer and enumerating the list of printers.
  constexpr char kEmptyPrinterName[] = "";
  std::string remote_id = GetRemoteIdForPrinterName(kEmptyPrinterName);
  auto iter = sandboxed_remotes_.find(remote_id);
  if (iter == sandboxed_remotes_.end()) {
    // Service not already available, so launch it now so that it will be
    // ready by the time the client gets to point of invoking a Mojo call.
    GetService(kEmptyPrinterName);
  } else {
    // Service already existed, possibly was recently marked for being reset
    // with a short timeout.  Ensure it has the long timeout to be available
    // across user interactions but to also get reclaimed should the user leave
    // it unused indefinitely.
    // Safe to use base::Unretained(this) since `this` is a global singleton
    // which never goes away.
    DVLOG(1) << "Updating to long idle timeout for print backend service id `"
             << remote_id << "`";
    mojo::Remote<printing::mojom::PrintBackendService>& service = iter->second;
    service.set_idle_handler(
        kClientsRegisteredResetOnIdleTimeout,
        base::BindRepeating(&PrintBackendServiceManager::OnIdleTimeout,
                            base::Unretained(this), /*sandboxed=*/true,
                            remote_id));

    // TODO(crbug.com/1225111)  Maybe need to issue a quick call here to get
    // adjusted timeout to take effect?  Ideally not, since there is supposed
    // to be an expected call "soon" after having registered.
  }

  return client_id;
}

void PrintBackendServiceManager::UnregisterClient(uint32_t id) {
  if (!clients_.erase(id)) {
    DVLOG(1) << "Unknown client ID " << id
             << ", is client being unregistered multiple times?";
    return;
  }
  VLOG(1) << "Unregistering client with ID " << id
          << " from print backend service.";

  if (!clients_.empty())
    return;

  // No more clients means that there is an opportunity to more aggressively
  // reclaim resources by letting service processes terminate.  Register a
  // short idle timeout with services.  This is preferred to just resetting
  // them immediately here, in case a user immediately reopens a Print Preview.
  for (auto& iter : sandboxed_remotes_) {
    const std::string& remote_id = iter.first;
    mojo::Remote<printing::mojom::PrintBackendService>& service = iter.second;
    UpdateServiceToShortIdleTimeout(service, /*sandboxed=*/true, remote_id);
  }
  for (auto& iter : unsandboxed_remotes_) {
    const std::string& remote_id = iter.first;
    mojo::Remote<printing::mojom::PrintBackendService>& service = iter.second;
    UpdateServiceToShortIdleTimeout(service, /*sandboxed=*/false, remote_id);
  }
}

const mojo::Remote<printing::mojom::PrintBackendService>&
PrintBackendServiceManager::GetService(const std::string& printer_name) {
  // Value of `is_sandboxed_service_` will be referenced during the service
  // launch by `ShouldSandboxPrintBackendService()` if the service is started
  // via `content::ServiceProcessHost::Launch()`.
  is_sandboxed_service_ = !PrinterDriverRequiresElevatedPrivilege(printer_name);

  if (sandboxed_service_remote_for_test_) {
    // The presence of a sandboxed remote for testing signals a testing
    // environment.  If no unsandboxed test service was provided for fallback
    // processing then use the sandboxed one for that as well.
    if (!is_sandboxed_service_ && unsandboxed_service_remote_for_test_)
      return *unsandboxed_service_remote_for_test_;

    return *sandboxed_service_remote_for_test_;
  }

  // Performance is improved if a service is launched ahead of the time it will
  // be needed by client callers.
  DCHECK(!clients_.empty());

  RemotesMap& remote =
      is_sandboxed_service_ ? sandboxed_remotes_ : unsandboxed_remotes_;

  // On the first print make note that so far no drivers have required fallback.
  static bool first_print = true;
  if (first_print) {
    DCHECK(is_sandboxed_service_);
    first_print = false;
    base::UmaHistogramBoolean(
        kPrintBackendRequiresElevatedPrivilegeHistogramName, /*sample=*/false);
  }

  std::string remote_id = GetRemoteIdForPrinterName(printer_name);
  auto iter = remote.find(remote_id);
  if (iter == remote.end()) {
    // First time for this `remote_id`.
    auto result = remote.emplace(
        printer_name, mojo::Remote<printing::mojom::PrintBackendService>());
    iter = result.first;
  }

  mojo::Remote<printing::mojom::PrintBackendService>& service = iter->second;
  if (!service) {
    VLOG(1) << "Launching print backend "
            << (is_sandboxed_service_ ? "sandboxed" : "unsandboxed") << " for '"
            << remote_id << "'";
    content::ServiceProcessHost::Launch(
        service.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_PRINT_BACKEND_SERVICE_NAME)
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) then we will drop our handle to the remote.
    // Safe to use base::Unretained(this) since `this` is a global singleton
    // which never goes away.
    service.set_disconnect_handler(base::BindOnce(
        &PrintBackendServiceManager::OnRemoteDisconnected,
        base::Unretained(this), is_sandboxed_service_, remote_id));

    // Beware of case where a user leaves a tab with a Print Preview open
    // indefinitely.  Use a long timeout against idleness to reclaim the unused
    // resources of an idle print backend service for this case.
    // Safe to use base::Unretained(this) since `this` is a global singleton
    // which never goes away.
    DVLOG(1) << "Updating to long idle timeout for print backend service id `"
             << remote_id << "`";
    service.set_idle_handler(
        kClientsRegisteredResetOnIdleTimeout,
        base::BindRepeating(&PrintBackendServiceManager::OnIdleTimeout,
                            base::Unretained(this), is_sandboxed_service_,
                            remote_id));

    // Initialize the new service for the desired locale.
    service->Init(g_browser_process->GetApplicationLocale());
  }

  return service;
}

void PrintBackendServiceManager::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  // Need to be able to run the callback either after a successful return from
  // the service or after the remote was disconnected, so save it here for
  // either eventuality.
  // Get a callback ID to represent this command.
  auto saved_callback_id = base::UnguessableToken::Create();

  // Note that `GetService()` will set state internally if this is sandboxed.
  const std::string kEmptyPrinterName;
  std::string remote_id = GetRemoteIdForPrinterName(kEmptyPrinterName);
  auto& service = GetService(kEmptyPrinterName);

  SaveCallback(GetRemoteSavedEnumeratePrintersCallbacks(is_sandboxed_service_),
               remote_id, saved_callback_id, std::move(callback));

  DVLOG(1) << "Sending EnumeratePrinters on remote `" << remote_id
           << "`, saved callback ID of " << saved_callback_id;
  service->EnumeratePrinters(
      base::BindOnce(&PrintBackendServiceManager::EnumeratePrintersDone,
                     base::Unretained(this), is_sandboxed_service_, remote_id,
                     saved_callback_id));
}

void PrintBackendServiceManager::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  // Need to be able to run the callback either after a successful return from
  // the service or after the remote was disconnected, so save it here for
  // either eventuality.
  // Get a callback ID to represent this command.
  auto saved_callback_id = base::UnguessableToken::Create();

  // Note that `GetService()` will set state internally if this is sandboxed.
  std::string remote_id = GetRemoteIdForPrinterName(printer_name);
  auto& service = GetService(printer_name);

  SaveCallback(GetRemoteSavedFetchCapabilitiesCallbacks(is_sandboxed_service_),
               remote_id, saved_callback_id, std::move(callback));

  if (!sandboxed_service_remote_for_test_) {
    // TODO(1227561)  Remove local call for driver info, don't want any
    // residual accesses left into the printer drivers from the browser
    // process.
    base::ScopedAllowBlocking allow_blocking;
    scoped_refptr<PrintBackend> print_backend =
        PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());
    crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
        print_backend->GetPrinterDriverInfo(printer_name));
  }

  DVLOG(1) << "Sending FetchCapabilities on remote `" << remote_id
           << "`, saved callback ID of " << saved_callback_id;
  service->FetchCapabilities(
      printer_name,
      base::BindOnce(&PrintBackendServiceManager::FetchCapabilitiesDone,
                     base::Unretained(this), is_sandboxed_service_, remote_id,
                     saved_callback_id));
}

void PrintBackendServiceManager::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  // Need to be able to run the callback either after a successful return from
  // the service or after the remote was disconnected, so save it here for
  // either eventuality.
  // Get a callback ID to represent this command.
  auto saved_callback_id = base::UnguessableToken::Create();

  // Note that `GetService()` will set state internally if this is sandboxed.
  std::string remote_id =
      GetRemoteIdForPrinterName(/*printer_name=*/std::string());
  auto& service = GetService(/*printer_name=*/std::string());

  SaveCallback(
      GetRemoteSavedGetDefaultPrinterNameCallbacks(is_sandboxed_service_),
      remote_id, saved_callback_id, std::move(callback));

  DVLOG(1) << "Sending GetDefaultPrinterName on remote `" << remote_id
           << "`, saved callback ID of " << saved_callback_id;
  service->GetDefaultPrinterName(
      base::BindOnce(&PrintBackendServiceManager::GetDefaultPrinterNameDone,
                     base::Unretained(this), is_sandboxed_service_, remote_id,
                     saved_callback_id));
}

void PrintBackendServiceManager::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  // Need to be able to run the callback either after a successful return from
  // the service or after the remote was disconnected, so save it here for
  // either eventuality.
  // Get a callback ID to represent this command.
  auto saved_callback_id = base::UnguessableToken::Create();

  // Note that `GetService()` will set state internally if this is sandboxed.
  std::string remote_id = GetRemoteIdForPrinterName(printer_name);
  auto& service = GetService(printer_name);

  SaveCallback(GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(
                   is_sandboxed_service_),
               remote_id, saved_callback_id, std::move(callback));

  if (!sandboxed_service_remote_for_test_) {
    // TODO(1227561)  Remove local call for driver info, don't want any
    // residual accesses left into the printer drivers from the browser
    // process.
    base::ScopedAllowBlocking allow_blocking;
    scoped_refptr<PrintBackend> print_backend =
        PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());
    crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
        print_backend->GetPrinterDriverInfo(printer_name));
  }

  DVLOG(1) << "Sending GetPrinterSemanticCapsAndDefaults on remote `"
           << remote_id << "`, saved callback ID of " << saved_callback_id;
  service->GetPrinterSemanticCapsAndDefaults(
      printer_name,
      base::BindOnce(
          &PrintBackendServiceManager::GetPrinterSemanticCapsAndDefaultsDone,
          base::Unretained(this), is_sandboxed_service_, remote_id,
          saved_callback_id));
}

bool PrintBackendServiceManager::PrinterDriverRequiresElevatedPrivilege(
    const std::string& printer_name) const {
  return drivers_requiring_elevated_privilege_.contains(printer_name);
}

void PrintBackendServiceManager::SetPrinterDriverRequiresElevatedPrivilege(
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
    mojo::Remote<printing::mojom::PrintBackendService>* remote) {
  sandboxed_service_remote_for_test_ = remote;
  sandboxed_service_remote_for_test_->set_disconnect_handler(base::BindOnce(
      &PrintBackendServiceManager::OnRemoteDisconnected, base::Unretained(this),
      /*sandboxed=*/true, /*remote_id=*/std::string()));
}

void PrintBackendServiceManager::SetServiceForFallbackTesting(
    mojo::Remote<printing::mojom::PrintBackendService>* remote) {
  unsandboxed_service_remote_for_test_ = remote;
  unsandboxed_service_remote_for_test_->set_disconnect_handler(base::BindOnce(
      &PrintBackendServiceManager::OnRemoteDisconnected, base::Unretained(this),
      /*sandboxed=*/false, /*remote_id=*/std::string()));
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

std::string PrintBackendServiceManager::GetRemoteIdForPrinterName(
    const std::string& printer_name) const {
  if (sandboxed_service_remote_for_test_) {
    return std::string();  // Test environment is always just one instance for
                           // all printers.
  }

#if defined(OS_WIN)
  // Windows drivers are not thread safe.  Use a
  // process per driver to prevent bad interactions
  // when interfacing to multiple drivers in parallel.
  // https://crbug.com/957242
  return printer_name;
#else
  return std::string();
#endif
}

void PrintBackendServiceManager::UpdateServiceToShortIdleTimeout(
    mojo::Remote<printing::mojom::PrintBackendService>& service,
    bool sandboxed,
    const std::string& remote_id) {
  DVLOG(1) << "Updating to short idle timeout for "
           << (sandboxed ? "sandboxed" : "unsandboxed")
           << " print backend service id `" << remote_id << "`";
  service.set_idle_handler(
      kNoClientsRegisteredResetOnIdleTimeout,
      base::BindRepeating(&PrintBackendServiceManager::OnIdleTimeout,
                          base::Unretained(this), sandboxed, remote_id));

  // TODO(crbug.com/1225111)  Make a superfluous call to the service, just to
  // cause an IPC that will in turn make the adjusted timeout value actually
  // take effect.
  service->Poke();
}

void PrintBackendServiceManager::OnIdleTimeout(bool sandboxed,
                                               const std::string& remote_id) {
  DVLOG(1) << "Print Backend service idle timeout for "
           << (sandboxed ? "sandboxed" : "unsandboxed") << " remote id "
           << remote_id;
  if (sandboxed) {
    sandboxed_remotes_.erase(remote_id);
  } else {
    unsandboxed_remotes_.erase(remote_id);
  }
}

void PrintBackendServiceManager::OnRemoteDisconnected(
    bool sandboxed,
    const std::string& remote_id) {
  DVLOG(1) << "Print Backend service disconnected for "
           << (sandboxed ? "sandboxed" : "unsandboxed") << " remote id "
           << remote_id;
  if (sandboxed) {
    sandboxed_remotes_.erase(remote_id);
  } else {
    unsandboxed_remotes_.erase(remote_id);
  }
  RunSavedCallbacks(
      GetRemoteSavedEnumeratePrintersCallbacks(sandboxed), remote_id,
      mojom::PrinterListResult::NewResultCode(mojom::ResultCode::kFailed));
  RunSavedCallbacks(GetRemoteSavedFetchCapabilitiesCallbacks(sandboxed),
                    remote_id,
                    mojom::PrinterCapsAndInfoResult::NewResultCode(
                        mojom::ResultCode::kFailed));
  RunSavedCallbacks(GetRemoteSavedGetDefaultPrinterNameCallbacks(sandboxed),
                    remote_id,
                    mojom::DefaultPrinterNameResult::NewResultCode(
                        mojom::ResultCode::kFailed));
  RunSavedCallbacks(
      GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(sandboxed),
      remote_id,
      mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(
          mojom::ResultCode::kFailed));
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

template <class T>
void PrintBackendServiceManager::SaveCallback(
    RemoteSavedCallbacks<T>& saved_callbacks,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    base::OnceCallback<void(mojo::StructPtr<T>)> callback) {
  saved_callbacks[remote_id].emplace(saved_callback_id, std::move(callback));
}

template <class T>
void PrintBackendServiceManager::ServiceCallbackDone(
    RemoteSavedCallbacks<T>& saved_callbacks,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    mojo::StructPtr<T> data) {
  auto found_callback_map = saved_callbacks.find(remote_id);
  DCHECK(found_callback_map != saved_callbacks.end());

  SavedCallbacks<T>& callback_map = found_callback_map->second;

  auto callback_entry = callback_map.find(saved_callback_id);
  DCHECK(callback_entry != callback_map.end());
  base::OnceCallback<void(mojo::StructPtr<T>)> callback =
      std::move(callback_entry->second);
  callback_map.erase(callback_entry);

  // Done disconnect wrapper management, propagate the callback.
  std::move(callback).Run(std::move(data));
}

void PrintBackendServiceManager::EnumeratePrintersDone(
    bool sandboxed,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    mojom::PrinterListResultPtr printer_list) {
  DVLOG(1) << "EnumeratePrinters completed for remote `" << remote_id
           << "` saved callback ID " << saved_callback_id;

  ServiceCallbackDone(GetRemoteSavedEnumeratePrintersCallbacks(sandboxed),
                      remote_id, saved_callback_id, std::move(printer_list));
}

void PrintBackendServiceManager::FetchCapabilitiesDone(
    bool sandboxed,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    mojom::PrinterCapsAndInfoResultPtr printer_caps_and_info) {
  DVLOG(1) << "FetchCapabilities completed for remote `" << remote_id
           << "` saved callback ID " << saved_callback_id;

  ServiceCallbackDone(GetRemoteSavedFetchCapabilitiesCallbacks(sandboxed),
                      remote_id, saved_callback_id,
                      std::move(printer_caps_and_info));
}

void PrintBackendServiceManager::GetDefaultPrinterNameDone(
    bool sandboxed,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    mojom::DefaultPrinterNameResultPtr printer_name) {
  DVLOG(1) << "GetDefaultPrinterName completed for remote `" << remote_id
           << "` saved callback ID " << saved_callback_id;

  ServiceCallbackDone(GetRemoteSavedGetDefaultPrinterNameCallbacks(sandboxed),
                      remote_id, saved_callback_id, std::move(printer_name));
}

void PrintBackendServiceManager::GetPrinterSemanticCapsAndDefaultsDone(
    bool sandboxed,
    const std::string& remote_id,
    const base::UnguessableToken& saved_callback_id,
    mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps) {
  DVLOG(1) << "GetPrinterSemanticCapsAndDefaults completed for remote `"
           << remote_id << "` saved callback ID " << saved_callback_id;

  ServiceCallbackDone(
      GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(sandboxed),
      remote_id, saved_callback_id, std::move(printer_caps));
}

template <class T>
void PrintBackendServiceManager::RunSavedCallbacks(
    RemoteSavedCallbacks<T>& saved_callbacks,
    const std::string& remote_id,
    mojo::StructPtr<T> result_to_clone) {
  auto found_callbacks_map = saved_callbacks.find(remote_id);
  if (found_callbacks_map == saved_callbacks.end())
    return;  // No callbacks to run.

  SavedCallbacks<T>& callbacks_map = found_callbacks_map->second;
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

}  // namespace printing
