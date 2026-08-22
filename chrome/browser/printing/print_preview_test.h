// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/idle/scoped_set_idle_state.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include <memory>

#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace printing {
class PrintBackendServiceTestImpl;
class TestPrintBackend;
}  // namespace printing

class PrintPreviewTest : public ChromeRenderViewHostTestHarness {
 public:
  PrintPreviewTest();

  PrintPreviewTest(const PrintPreviewTest&) = delete;
  PrintPreviewTest& operator=(const PrintPreviewTest&) = delete;

  ~PrintPreviewTest() override;

  void SetUp() override;
  void TearDown() override;

 private:
  ui::ScopedSetIdleState scoped_idle_state_{ui::IDLE_STATE_ACTIVE};
  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<printing::mojom::PrintBackendService> test_remote_;
  std::unique_ptr<printing::PrintBackendServiceTestImpl> print_backend_service_;
#endif
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
