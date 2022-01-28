// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h"
#include "chrome/browser/file_util_service.h"
#include "mojo/public/cpp/system/data_pipe.h"

// XzExtractor performs XZ extraction by using a sandboxed service
// (FileUtilService). Extracted data will be handed to SingleFileTarReader,
// which is basically a class that drops tar header and padding for the given
// tar stream, and will be written to extracted.bin.

namespace extensions {
namespace image_writer {

namespace {

constexpr base::FilePath::StringPieceType kExtractedBinFileName =
    FILE_PATH_LITERAL("extracted.bin");

// https://tukaani.org/xz/xz-file-format-1.0.4.txt
constexpr uint8_t kExpectedMagic[6] = {0xfd, '7', 'z', 'X', 'Z', 0x00};

}  // namespace

bool XzExtractor::IsXzFile(const base::FilePath& image_path) {
  base::File infile(image_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                    base::File::FLAG_WIN_SHARE_DELETE);
  if (!infile.IsValid())
    return false;

  constexpr size_t kExpectedSize = sizeof(kExpectedMagic);
  char actual_magic[kExpectedSize] = {};
  if (infile.ReadAtCurrentPos(actual_magic, kExpectedSize) != kExpectedSize)
    return false;

  return std::equal(
      reinterpret_cast<const char*>(kExpectedMagic),
      reinterpret_cast<const char*>(kExpectedMagic + kExpectedSize),
      actual_magic);
}

// static
void XzExtractor::Extract(ExtractionProperties properties) {
  // XzExtractor manages its own lifetime, and will delete itself when it
  // completes.
  XzExtractor* extractor = new XzExtractor(std::move(properties));
  extractor->ExtractImpl();
}

XzExtractor::XzExtractor(ExtractionProperties properties)
    : tar_reader_(this),
      xz_producer_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
      tar_consumer_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
      properties_(std::move(properties)) {}

XzExtractor::~XzExtractor() = default;

void XzExtractor::ExtractImpl() {
  service_.Bind(LaunchFileUtilService());
  service_->BindXzFileExtractor(
      remote_xz_file_extractor_.BindNewPipeAndPassReceiver());

  mojo::ScopedDataPipeConsumerHandle xz_consumer;
  mojo::ScopedDataPipeProducerHandle tar_producer;

  if (mojo::CreateDataPipe(nullptr, xz_producer_, xz_consumer) !=
          MOJO_RESULT_OK ||
      mojo::CreateDataPipe(nullptr, tar_producer, tar_consumer_) !=
          MOJO_RESULT_OK) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }

  // base::Unretained(this) is safe here because callback won't be called once
  // |remote_xz_file_extractor_| is destroyed.
  remote_xz_file_extractor_->Extract(
      std::move(xz_consumer), std::move(tar_producer),
      base::BindOnce(&XzExtractor::OnRemoteFinished, base::Unretained(this)));

  infile_.Initialize(properties_.image_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ |
                         base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                         base::File::FLAG_WIN_SHARE_DELETE);
  if (!infile_.IsValid()) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(kExtractedBinFileName);
  outfile_.Initialize(out_image_path, base::File::FLAG_CREATE_ALWAYS |
                                          base::File::FLAG_WRITE |
                                          base::File::FLAG_WIN_EXCLUSIVE_READ |
                                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!outfile_.IsValid()) {
    RunFailureCallbackAndDeleteThis(error::kTempFileError);
    return;
  }
  std::move(properties_.open_callback).Run(out_image_path);

  // base::Unretained(this) is safe here because callback won't be called once
  // |xz_producer_watcher_| is destroyed.
  xz_producer_watcher_.Watch(
      xz_producer_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&XzExtractor::OnXzWritable, base::Unretained(this)));

  // base::Unretained(this) is safe here because callback won't be called once
  // |tar_consumer_watcher_| is destroyed.
  tar_consumer_watcher_.Watch(
      tar_consumer_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&XzExtractor::OnTarReadable, base::Unretained(this)));
}

void XzExtractor::OnXzWritable(MojoResult /* result */) {
  char* data = nullptr;
  uint32_t size = 0;
  MojoResult result = xz_producer_->BeginWriteData(
      reinterpret_cast<void**>(&data), &size, MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    return;
  }
  if (result != MOJO_RESULT_OK) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }
  const int bytes_read = infile_.ReadAtCurrentPos(data, size);
  if (bytes_read < 0) {
    xz_producer_->EndWriteData(0);
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }
  result = xz_producer_->EndWriteData(bytes_read);
  if (result != MOJO_RESULT_OK) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }
}

void XzExtractor::OnTarReadable(MojoResult /* result */) {
  SingleFileTarReader::Result result = tar_reader_.ExtractChunk();
  if (result == SingleFileTarReader::Result::kShouldWait)
    return;
  if (result == SingleFileTarReader::Result::kFailure) {
    RunFailureCallbackAndDeleteThis(tar_reader_.error_id());
    return;
  }

  // TODO(tetsui): Also check it's the end of XZ.
  if (tar_reader_.IsComplete()) {
    auto complete_callback = std::move(properties_.complete_callback);
    delete this;
    std::move(complete_callback).Run();
    return;
  }

  properties_.progress_callback.Run(tar_reader_.total_bytes().value(),
                                    tar_reader_.curr_bytes());
}

void XzExtractor::OnRemoteFinished(bool success) {
  if (!success) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
  }
}

void XzExtractor::RunFailureCallbackAndDeleteThis(std::string error_id) {
  auto failure_callback = std::move(properties_.failure_callback);
  delete this;
  std::move(failure_callback).Run(error_id);
}

SingleFileTarReader::Result XzExtractor::ReadTarFile(char* data,
                                                     uint32_t* size,
                                                     std::string* error_id) {
  MojoResult result =
      tar_consumer_->ReadData(data, size, MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT)
    return SingleFileTarReader::Result::kShouldWait;
  if (result == MOJO_RESULT_OK)
    return SingleFileTarReader::Result::kSuccess;
  *error_id = error::kUnzipGenericError;
  return SingleFileTarReader::Result::kFailure;
}

bool XzExtractor::WriteContents(const char* data,
                                int size,
                                std::string* error_id) {
  const int bytes_written = outfile_.WriteAtCurrentPos(data, size);
  if (bytes_written < 0 || bytes_written != size) {
    *error_id = error::kTempFileError;
    return false;
  }
  return true;
}

}  // namespace image_writer
}  // namespace extensions
