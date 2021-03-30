// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_H_

#include <string>

#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Acquires a remote handle to the Print Backend Service instance, launching a
// process to host the service if necessary.
const mojo::Remote<printing::mojom::PrintBackendService>&
GetPrintBackendService(const std::string& locale,
                       const std::string& printer_name);

// Test support to override the print backend service for testing.  Caller
// retains ownership of `remote`.
void SetPrintBackendServiceForTesting(
    mojo::Remote<printing::mojom::PrintBackendService>* remote);

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_H_
