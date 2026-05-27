// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_browsertest.h"

#include "printing/backend/test_print_backend.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#endif

namespace printing {

PrintPreviewBrowserTest::PrintPreviewBrowserTest() = default;
PrintPreviewBrowserTest::~PrintPreviewBrowserTest() = default;

void PrintPreviewBrowserTest::SetUp() {
  test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
  PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());
  InProcessBrowserTest::SetUp();
}

void PrintPreviewBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (IsOopPrintingEnabled()) {
    print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
        test_remote_, test_print_backend_, /*sandboxed=*/true);
  }
#endif
}

void PrintPreviewBrowserTest::TearDown() {
  InProcessBrowserTest::TearDown();
  PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
}

}  // namespace printing
