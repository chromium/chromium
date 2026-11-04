// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/pdf_content_reader.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/buffer.h"
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

// A `SaveDataBufferHandler` that always replies with a shared-memory-backed
// `BigBuffer` and retains a writable mapping of the same region so the test can
// mutate it after the reply has been delivered.
class SharedMemorySaveDataBufferHandler
    : public pdf::mojom::SaveDataBufferHandler {
 public:
  explicit SharedMemorySaveDataBufferHandler(
      mojo::PendingReceiver<pdf::mojom::SaveDataBufferHandler> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SetData(std::string data) { data_ = std::move(data); }

  void Read(uint32_t offset,
            uint32_t block_size,
            ReadCallback callback) override {
    ASSERT_EQ(offset, 0u);
    mojo::ScopedSharedBufferHandle handle =
        mojo::SharedBufferHandle::Create(data_.size());
    ASSERT_TRUE(handle.is_valid());
    retained_mapping_ = handle->Map(data_.size());
    ASSERT_TRUE(retained_mapping_);
    UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(retained_mapping_.get()),
                              data_.size()))
        .copy_from(base::as_bytes(base::span(data_)));
    mojo::ScopedSharedBufferHandle dup =
        handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);
    ASSERT_TRUE(dup.is_valid());
    mojo_base::internal::BigBufferSharedMemoryRegion region(std::move(dup),
                                                            data_.size());
    std::move(callback).Run(mojo_base::BigBuffer(std::move(region)));
  }

  void OverwriteFirstByte(uint8_t value) {
    ASSERT_TRUE(retained_mapping_);
    *static_cast<uint8_t*>(retained_mapping_.get()) = value;
  }

 private:
  mojo::Receiver<pdf::mojom::SaveDataBufferHandler> receiver_;
  std::string data_;
  mojo::ScopedSharedBufferMapping retained_mapping_;
};

class PDFContentReaderTest : public ChromeRenderViewHostTestHarness {
 protected:
  PDFContentReaderTest() = default;

  void OpenReader(PDFContentReader* reader,
                  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote,
                  uint32_t total_file_size) {
    base::test::TestFuture<bool> future;
    reader->OnOpen(future.GetCallback(),
                   pdf::mojom::SaveDataBufferHandlerGetResult::New(
                       std::move(remote), total_file_size));
    ASSERT_TRUE(future.Get());
  }

  mojo_base::BigBuffer Read(PDFContentReader* reader,
                            uint32_t offset,
                            uint32_t size) {
    base::test::TestFuture<mojo_base::BigBuffer> future;
    reader->Read(offset, size, future.GetCallback());
    return future.Take();
  }
};

TEST_F(PDFContentReaderTest, ReadValidPdf) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  mojo_base::BigBuffer result = Read(reader.get(), 0, kData.size());
  EXPECT_EQ(base::as_string_view(result), kData);
}

TEST_F(PDFContentReaderTest, ReadInvalidHeader) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "Not a PDF";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  mojo_base::BigBuffer result = Read(reader.get(), 0, kData.size());
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadMissingEofMarker) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  mojo_base::BigBuffer result = Read(reader.get(), 0, kData.size());
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadNonZeroOffsetWithoutHeaderValidation) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  // Reading at a non-zero offset before the header has been validated must
  // fail.
  mojo_base::BigBuffer result = Read(reader.get(), 1, kData.size() - 1);
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadOutOfOrder) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  ASSERT_EQ(Read(reader.get(), 0, 8).size(), 8u);
  // Skipping ahead past the next expected offset must fail.
  EXPECT_EQ(Read(reader.get(), 16, 7).size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadInChunks) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  EXPECT_EQ(base::as_string_view(Read(reader.get(), 0, 8)), kData.substr(0, 8));
  EXPECT_EQ(base::as_string_view(Read(reader.get(), 8, 8)), kData.substr(8, 8));
  EXPECT_EQ(base::as_string_view(Read(reader.get(), 16, 7)),
            kData.substr(16, 7));
}

TEST_F(PDFContentReaderTest, ReadInChunksMissingEofMarker) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 some content..";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  EXPECT_EQ(Read(reader.get(), 0, 8).size(), 8u);
  EXPECT_EQ(Read(reader.get(), 8, 8).size(), 8u);
  // The final chunk has no end-of-file marker.
  EXPECT_EQ(Read(reader.get(), 16, 7).size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadInChunksEofMarkerSpansChunkBoundary) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  // The end-of-file marker straddles the boundary between the second and third
  // chunks, with only trailing whitespace in the third chunk.
  static constexpr std::string_view kData = "%PDF-1.5 ..%%EOF\n\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  EXPECT_EQ(Read(reader.get(), 0, 8).size(), 8u);
  EXPECT_EQ(Read(reader.get(), 8, 8).size(), 8u);
  EXPECT_EQ(base::as_string_view(Read(reader.get(), 16, 2)), "\n\n");
}

TEST_F(PDFContentReaderTest, ReadRestartFromZero) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  EXPECT_EQ(Read(reader.get(), 0, 8).size(), 8u);
  EXPECT_EQ(Read(reader.get(), 8, 8).size(), 8u);
  // Restarting the read sequence from the beginning is allowed.
  EXPECT_EQ(Read(reader.get(), 0, kData.size()).size(), kData.size());
}

TEST_F(PDFContentReaderTest, ReadExceedsTotalFileSize) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  MockSaveDataBufferHandler handler(remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  // Advertise a smaller total file size than the data the handler will return.
  OpenReader(reader.get(), std::move(remote), kData.size() - 1);

  mojo_base::BigBuffer result = Read(reader.get(), 0, kData.size());
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(PDFContentReaderTest, ReadCopiesSharedMemoryBuffer) {
  auto reader = std::make_unique<PDFContentReader>(
      main_rfh(), pdf::mojom::SaveRequestType::kOriginal);

  static constexpr std::string_view kData = "%PDF-1.5 content\n%%EOF\n";
  mojo::PendingRemote<pdf::mojom::SaveDataBufferHandler> remote;
  SharedMemorySaveDataBufferHandler handler(
      remote.InitWithNewPipeAndPassReceiver());
  handler.SetData(std::string(kData));
  OpenReader(reader.get(), std::move(remote), kData.size());

  mojo_base::BigBuffer result = Read(reader.get(), 0, kData.size());
  ASSERT_EQ(base::as_string_view(result), kData);

  // Mutating the shared memory backing the reply must not affect the buffer
  // returned to the caller.
  handler.OverwriteFirstByte('X');
  EXPECT_EQ(base::as_string_view(result), kData);
}

}  // namespace save_to_drive
