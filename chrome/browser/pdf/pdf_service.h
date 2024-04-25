// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_SERVICE_H_
#define CHROME_BROWSER_PDF_PDF_SERVICE_H_

#include "chrome/services/pdf/public/mojom/pdf_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// Returns the singleton remote handle to a sandboxed PDF Service instance, and
// launch a process if necessary. The process will be terminated when there are
// no in-flight messages and no other interfaces bound through the remote.
const mojo::Remote<pdf::mojom::PdfService>& GetPdfService();

// Launches a new process and returns a remote handle to a sandboxed PDF Service
// instance. Callers should terminate the process when it is no longer needed.
mojo::Remote<pdf::mojom::PdfService> LaunchPdfService();

#endif  // CHROME_BROWSER_PDF_PDF_SERVICE_H_
