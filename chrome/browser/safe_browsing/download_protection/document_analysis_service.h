// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOCUMENT_ANALYSIS_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOCUMENT_ANALYSIS_SERVICE_H_

#include "chrome/services/file_util/public/mojom/document_analysis_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// Launches a new instance of the DocumentAnalysis service in an isolated,
// sandboxed process and returns a remote interface to control the service.
// The lifetime of the process is tied to that of the remote.
mojo::PendingRemote<chrome::mojom::DocumentAnalysisService>
LaunchDocumentAnalysisService();

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOCUMENT_ANALYSIS_SERVICE_H_
