// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Amount of idle time to wait before resetting the connection to the service.
constexpr base::TimeDelta kResetOnIdleTimeout =
    base::TimeDelta::FromSeconds(20);

// `PrintBackendService` override for testing.
mojo::Remote<printing::mojom::PrintBackendService>*
    g_print_backend_service_for_test = nullptr;

}  // namespace

const mojo::Remote<printing::mojom::PrintBackendService>&
GetPrintBackendService(const std::string& locale,
                       const std::string& printer_name) {
  static base::NoDestructor<base::flat_map<
      std::string, mojo::Remote<printing::mojom::PrintBackendService>>>
      remotes;

  if (g_print_backend_service_for_test)
    return *g_print_backend_service_for_test;

  std::string remote_id;
#if defined(OS_WIN)
  // Windows drivers are not thread safe.  Use a process per driver to prevent
  // bad interactions when interfacing to multiple drivers in parallel.
  // https://crbug.com/957242
  remote_id = printer_name;
#endif
  auto iter = remotes->find(remote_id);
  if (iter == remotes->end()) {
    // First time for this `remote_id`.
    auto result = remotes->emplace(
        printer_name, mojo::Remote<printing::mojom::PrintBackendService>());
    iter = result.first;
  }

  mojo::Remote<printing::mojom::PrintBackendService>& service = iter->second;
  if (!service) {
    content::ServiceProcessHost::Launch(
        service.BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_PRINT_BACKEND_SERVICE_NAME)
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) or goes idle for a short period of time -- meaning
    // there are no in-flight messages and no other interfaces bound through
    // this one -- then we will reset `remote`, causing the service process to
    // be terminated if it isn't already.
    service.reset_on_disconnect();
    // TODO(crbug.com/809738) Interactions with the service should be expected
    // as long as any Print Preview dialogs are open (and there could be more
    // than one preview open at a time).  Keeping the service present as long
    // as those are open would help provide a more responsive experience for
    // the user.  For now, to ensure that this process doesn't stick around
    // forever we make it go away after a short delay of idleness, but that
    // should be adjusted to happen only after all UI references have been
    // removed.
    service.reset_on_idle_timeout(kResetOnIdleTimeout);

    // Initialize the new service for the desired locale.
    service->Init(locale);
  }

  return service;
}

void SetPrintBackendServiceForTesting(
    mojo::Remote<printing::mojom::PrintBackendService>* remote) {
  g_print_backend_service_for_test = remote;
}
