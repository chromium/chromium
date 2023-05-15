// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::shimless_rma {

// Test-fake implementation of QRImageGenerator; the real implementation
// can't be used in these tests because it may require spawning a service
// process.
void GenerateFakeQRCode(
    qrcode_generator::mojom::GenerateQRCodeRequestPtr request,
    qrcode_generator::QRImageGenerator::ResponseCallback callback) {
  qrcode_generator::mojom::GenerateQRCodeResponsePtr response =
      qrcode_generator::mojom::GenerateQRCodeResponse::New();
  response->error_code = qrcode_generator::mojom::QRCodeGeneratorError::NONE;
  response->bitmap.allocN32Pixels(16, 16);

  std::move(callback).Run(std::move(response));
}

class ChromeShimlessRmaDelegateTest : public testing::Test {
 public:
  ChromeShimlessRmaDelegateTest()
      : chrome_shimless_rma_delegate_(ChromeShimlessRmaDelegate(nullptr)),
        task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}
  ~ChromeShimlessRmaDelegateTest() override = default;

  void SetUp() override {
    chrome_shimless_rma_delegate_.SetQRCodeServiceForTesting(
        base::BindRepeating(&GenerateFakeQRCode));
  }

  void TearDown() override {}

 protected:
  ChromeShimlessRmaDelegate chrome_shimless_rma_delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Validates a QrCode Bitmap is correctly converted to a string.
TEST_F(ChromeShimlessRmaDelegateTest, GenerateQrCode) {
  base::RunLoop run_loop;
  chrome_shimless_rma_delegate_.GenerateQrCode(
      "www.sample-url.com",
      base::BindLambdaForTesting([&](const std::string& qr_code_image) {
        EXPECT_FALSE(qr_code_image.empty());
      }));
  run_loop.RunUntilIdle();
}

}  // namespace ash::shimless_rma
