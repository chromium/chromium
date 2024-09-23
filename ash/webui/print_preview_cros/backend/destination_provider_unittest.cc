// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/destination_provider.h"

#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::print_preview {

class DestinationProviderTest : public testing::Test {
 public:
  DestinationProviderTest() = default;
  ~DestinationProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    destination_provider_.BindInterface(
        destination_provider_remote_.BindNewPipeAndPassReceiver());
    destination_provider_remote_.FlushForTesting();
  }

  mojom::CapabilitiesPtr FetchCapabilities(
      const std::string& destination_id,
      ::printing::mojom::PrinterType printer_type) {
    base::test::TestFuture<mojom::CapabilitiesPtr> future;
    destination_provider_remote_->FetchCapabilities(
        destination_id, printer_type, future.GetCallback());
    return future.Take();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  DestinationProvider destination_provider_;
  mojo::Remote<mojom::DestinationProvider> destination_provider_remote_;
};

// Verify requesting PDF printing capabilities returns the expected default
// settings.
TEST_F(DestinationProviderTest, FetchPdfCapabilities) {
  const std::string destination_id = "PDF_printer";
  const mojom::CapabilitiesPtr caps =
      FetchCapabilities(destination_id, ::printing::mojom::PrinterType::kPdf);

  EXPECT_EQ(destination_id, caps->destination_id);
  EXPECT_FALSE(caps->page_orientation->options.empty());
  EXPECT_FALSE(caps->color->options.empty());
  EXPECT_FALSE(caps->media_size->options.empty());
  EXPECT_FALSE(caps->dpi->options.empty());
}

}  // namespace ash::printing::print_preview
