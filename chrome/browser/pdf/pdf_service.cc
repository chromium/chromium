// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_service.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "content/public/browser/service_process_host.h"

const mojo::Remote<pdf::mojom::PdfService>& GetPdfService() {
  static base::NoDestructor<mojo::Remote<pdf::mojom::PdfService>> remote;
  if (!*remote) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_PDF_SERVICE_NAME)
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) or goes idle for a short period of time -- meaning there
    // are no in-flight messages and no other interfaces bound through this
    // one -- then we will reset `remote`, causing the service process to be
    // terminated if it isn't already.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(base::Seconds(5));
  }

  return *remote;
}

mojo::Remote<pdf::mojom::PdfService> LaunchPdfService() {
  return content::ServiceProcessHost::Launch<pdf::mojom::PdfService>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_PDF_SERVICE_NAME)
          .Pass());
}
