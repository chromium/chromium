// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {

namespace {
constexpr base::FilePath::CharType kExtractedBinFileName[] =
    FILE_PATH_LITERAL("extracted.bin");
}  // namespace

bool TarExtractor::IsTarFile(const base::FilePath& image_path) {
  // TODO(tetsui): Check the file header instead of the extension.
  return image_path.Extension() == FILE_PATH_LITERAL(".tar");
}

// static
void TarExtractor::Extract(ExtractionProperties properties) {
  // TarExtractor manages its own lifetime, and will delete itself when it
  // completes.
  TarExtractor* extractor = new TarExtractor(std::move(properties));
  extractor->ExtractImpl();
}

TarExtractor::TarExtractor(ExtractionProperties properties)
    : tar_reader_(this), properties_(std::move(properties)) {}

TarExtractor::~TarExtractor() = default;

int TarExtractor::ReadTarFile(char* data, int size, std::string* error_id) {
  const int bytes_read = infile_.ReadAtCurrentPos(data, size);
  if (bytes_read < 0) {
    *error_id = error::kUnzipGenericError;
  }
  return bytes_read;
}

bool TarExtractor::WriteContents(const char* data,
                                 int size,
                                 std::string* error_id) {
  const int bytes_written = outfile_.WriteAtCurrentPos(data, size);
  if (bytes_written < 0 || bytes_written != size) {
    *error_id = error::kTempFileError;
    return false;
  }
  return true;
}

void TarExtractor::ExtractImpl() {
  infile_.Initialize(properties_.image_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!infile_.IsValid()) {
    std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
    delete this;
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(kExtractedBinFileName);
  outfile_.Initialize(out_image_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!outfile_.IsValid()) {
    std::move(properties_.failure_callback).Run(error::kTempFileError);
    delete this;
    return;
  }
  std::move(properties_.open_callback).Run(out_image_path);

  ExtractChunk();
}

void TarExtractor::ExtractChunk() {
  if (!tar_reader_.ExtractChunk()) {
    std::move(properties_.failure_callback).Run(tar_reader_.error_id());
    delete this;
    return;
  }

  if (tar_reader_.IsComplete()) {
    std::move(properties_.complete_callback).Run();
    delete this;
    return;
  }

  properties_.progress_callback.Run(tar_reader_.total_bytes(),
                                    tar_reader_.curr_bytes());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(&TarExtractor::ExtractChunk,
                                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace image_writer
}  // namespace extensions
