// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTING_SERVICE_H_
#define CHROME_BROWSER_PRINTING_PRINTING_SERVICE_H_

#include "chrome/services/printing/public/mojom/printing_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// Acquires a remote handle to the sandboxed Printing Service
// instance, launching a process to host the service if necessary.
const mojo::Remote<printing::mojom::PrintingService>& GetPrintingService();

#endif  // CHROME_BROWSER_PRINTING_PRINTING_SERVICE_H_
