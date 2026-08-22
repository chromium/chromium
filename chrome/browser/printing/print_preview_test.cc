// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_test.h"

#include "printing/backend/test_print_backend.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#endif

PrintPreviewTest::PrintPreviewTest() = default;

PrintPreviewTest::~PrintPreviewTest() = default;

void PrintPreviewTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  test_print_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
  printing::PrintBackend::SetPrintBackendForTesting(test_print_backend_.get());

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::IsOopPrintingEnabled()) {
    print_backend_service_ =
        printing::PrintBackendServiceTestImpl::LaunchForTesting(
            test_remote_, test_print_backend_, /*sandboxed=*/true);
  }
#endif
}

void PrintPreviewTest::TearDown() {
  printing::PrintBackend::SetPrintBackendForTesting(/*print_backend=*/nullptr);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  print_backend_service_.reset();
  test_remote_.reset();
#endif
  test_print_backend_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}
