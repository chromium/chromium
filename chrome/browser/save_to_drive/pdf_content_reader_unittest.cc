// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/pdf_content_reader.h"

#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

class MockSaveDataBufferHandler : public pdf::mojom::SaveDataBufferHandler {
 public:
  explicit MockSaveDataBufferHandler(
      mojo::PendingReceiver<pdf::mojom::SaveDataBufferHandler> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SetData(std::string data) { data_ = std::move(data); }

  void Read(uint32_t offset,
            uint32_t block_size,
            ReadCallback callback) override {
    if (offset >= data_.size()) {
      std::move(callback).Run(mojo_base::BigBuffer());
      return;
    }
    size_t size =
        std::min(static_cast<size_t>(block_size), data_.size() - offset);
    mojo_base::BigBuffer buffer(size);
    UNSAFE_BUFFERS(base::span(buffer.data(), buffer.size()))
        .copy_from(base::as_bytes(base::span(data_).subspan(offset, size)));
    std::move(callback).Run(std::move(buffer));
  }

 private:
  mojo::Receiver<pdf::mojom::SaveDataBufferHandler> receiver_;
  std::string data_;
};

class PDFContentReaderTest : public ChromeRenderViewHostTestHarness {
 protected:
  PDFContentReaderTest() = default;

  void SetRemoteBufferHandler(
      PDFContentReader* reader,
      mojo::Remote<pdf::mojom::SaveDataBufferHandler> remote) {
    reader->remote_buffer_handler_ = std::move(remote);
  }
};

TEST_F(PDFContentReaderTest, ReadValidPdf) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  mojo::Remote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.BindNewPipeAndPassReceiver());
  handler.SetData("%PDF-1.5 content");
  SetRemoteBufferHandler(reader.get(), std::move(remote));

  base::test::TestFuture<mojo_base::BigBuffer> future;
  reader->Read(0, 16, future.GetCallback());

  mojo_base::BigBuffer result = future.Take();
  EXPECT_EQ(result.size(), 16u);
  EXPECT_EQ(base::as_string_view(result), "%PDF-1.5 content");
}

TEST_F(PDFContentReaderTest, ReadInvalidPdf) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  mojo::Remote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.BindNewPipeAndPassReceiver());
  handler.SetData("Not a PDF");
  SetRemoteBufferHandler(reader.get(), std::move(remote));

  base::test::TestFuture<mojo_base::BigBuffer> future;
  reader->Read(0, 9, future.GetCallback());

  mojo_base::BigBuffer result = future.Take();
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadNonZeroOffset) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  mojo::Remote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.BindNewPipeAndPassReceiver());
  handler.SetData("Not a PDF");
  SetRemoteBufferHandler(reader.get(), std::move(remote));

  base::test::TestFuture<mojo_base::BigBuffer> future;
  reader->Read(1, 8, future.GetCallback());

  mojo_base::BigBuffer result = future.Take();
  EXPECT_EQ(result.size(), 8u);
  EXPECT_EQ(base::as_string_view(result), "ot a PDF");
}

}  // namespace save_to_drive
