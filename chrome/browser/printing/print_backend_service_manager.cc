// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_manager.h"

#include <string>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace printing {

namespace {

// Histogram name for capturing if any printer drivers were encountered that
// required fallback to workaround an access-denied error.  Determining if this
// happens in the wild would be the impetus to pursue further efforts to
// identify and possibly better rectify such cases.
constexpr char kPrintBackendRequiresElevatedPrivilegeHistogramName[] =
    "Printing.PrintBackend.DriversRequiringElevatedPrivilegeEncountered";

// Amount of idle time to wait before resetting the connection to the service.
constexpr base::TimeDelta kResetOnIdleTimeout =
    base::TimeDelta::FromSeconds(20);

PrintBackendServiceManager* g_print_backend_service_manager_singleton = nullptr;

}  // namespace

PrintBackendServiceManager::PrintBackendServiceManager() = default;

PrintBackendServiceManager::~PrintBackendServiceManager() = default;

bool PrintBackendServiceManager::ShouldSandboxPrintBackendService() const {
  return is_sandboxed_service_;
}

const mojo::Remote<printing::mojom::PrintBackendService>&
PrintBackendServiceManager::GetService(const std::string& locale,
                                       const std::string& printer_name) {
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

  std::string remote_id;
#if defined(OS_WIN)
  // Windows drivers are not thread safe.  Use a process per driver to prevent
  // bad interactions when interfacing to multiple drivers in parallel.
  // https://crbug.com/957242
  remote_id = printer_name;
#endif
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

    // TODO(crbug.com/809738) Interactions with the service should be expected
    // as long as any Print Preview dialogs are open (and there could be more
    // than one preview open at a time).  Keeping the service present as long
    // as those are open would help provide a more responsive experience for
    // the user.  For now, to ensure that this process doesn't stick around
    // forever we make it go away after a short delay of idleness, but that
    // should be adjusted to happen only after all UI references have been
    // removed.
    // Safe to use base::Unretained(this) since `this` is a global singleton
    // which never goes away.
    service.set_idle_handler(
        kResetOnIdleTimeout,
        base::BindRepeating(&PrintBackendServiceManager::OnIdleTimeout,
                            base::Unretained(this), is_sandboxed_service_,
                            remote_id));

    // Initialize the new service for the desired locale.
    service->Init(locale);
  }

  return service;
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
}

void PrintBackendServiceManager::SetServiceForFallbackTesting(
    mojo::Remote<printing::mojom::PrintBackendService>* remote) {
  unsandboxed_service_remote_for_test_ = remote;
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

}  // namespace printing
