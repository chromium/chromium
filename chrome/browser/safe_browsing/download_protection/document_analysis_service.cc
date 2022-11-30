// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/document_analysis_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/file_util/public/mojom/document_analysis_service.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

mojo::PendingRemote<chrome::mojom::DocumentAnalysisService>
LaunchDocumentAnalysisService() {
  mojo::PendingRemote<chrome::mojom::DocumentAnalysisService> remote;
  content::ServiceProcessHost::Launch<chrome::mojom::DocumentAnalysisService>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_SERVICE_PROCESS_DOCUMENT_ANALYSIS_NAME)
          .Pass());
  return remote;
}
