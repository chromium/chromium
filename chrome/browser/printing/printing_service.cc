// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_service.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/service_process_host.h"

const mojo::Remote<printing::mojom::PrintingService>& GetPrintingService() {
  static base::NoDestructor<mojo::Remote<printing::mojom::PrintingService>>
      remote;
  if (!*remote) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_PRINTING_SERVICE_NAME)
            .WithSandboxType(service_manager::SANDBOX_TYPE_UTILITY)
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) or goes idle for a short period of time -- meaning there
    // are no in-flight messages and no other interfaces bound through this
    // one -- then we will reset |remote|, causing the service process to be
    // terminated if it isn't already.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(base::TimeDelta::FromSeconds(5));
  }

  return *remote;
}
