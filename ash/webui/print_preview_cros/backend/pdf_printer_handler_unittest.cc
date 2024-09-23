// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/pdf_printer_handler.h"

#include <memory>

#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::print_preview {

class PdfPrinterHandlerTest : public testing::Test {
 public:
  PdfPrinterHandlerTest() = default;
  ~PdfPrinterHandlerTest() override = default;

 protected:
  PdfPrinterHandler handler_;
};

// Verify requesting capabilities returns the expected default settings.
TEST_F(PdfPrinterHandlerTest, FetchCapabilities) {
  const std::string destination_id = "PDF_printer";
  const mojom::CapabilitiesPtr caps =
      handler_.FetchCapabilities(destination_id);

  EXPECT_EQ(destination_id, caps->destination_id);
  EXPECT_EQ(3u, caps->page_orientation->options.size());
  EXPECT_EQ(1u, caps->color->options.size());
  EXPECT_EQ(1u, caps->media_size->options.size());
  EXPECT_EQ(1u, caps->dpi->options.size());
}

}  // namespace ash::printing::print_preview
