// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_SERVICE_H_
#define CHROME_BROWSER_PDF_PDF_SERVICE_H_

#include "chrome/services/pdf/public/mojom/pdf_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// Acquires a remote handle to the sandboxed PDF Service instance, launching
// a process to host the service if necessary.
const mojo::Remote<pdf::mojom::PdfService>& GetPdfService();

#endif  // CHROME_BROWSER_PDF_PDF_SERVICE_H_
