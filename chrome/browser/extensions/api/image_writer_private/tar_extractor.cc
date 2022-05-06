// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"

#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"

namespace extensions {
namespace image_writer {

namespace {
constexpr base::FilePath::CharType kExtractedBinFileName[] =
    FILE_PATH_LITERAL("extracted.bin");

// https://www.gnu.org/software/tar/manual/html_node/Standard.html
constexpr char kExpectedMagic[5] = {'u', 's', 't', 'a', 'r'};
constexpr int kMagicOffset = 257;

}  // namespace

bool TarExtractor::IsTarFile(const base::FilePath& image_path) {
  base::File infile(image_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                    base::File::FLAG_WIN_SHARE_DELETE);
  if (!infile.IsValid())
    return false;

  // Tar header record is always 512 bytes, so if the file is shorter than that,
  // it's not tar.
  char header[512] = {};
  if (infile.ReadAtCurrentPos(header, sizeof(header)) != sizeof(header))
    return false;

  return std::equal(kExpectedMagic, kExpectedMagic + sizeof(kExpectedMagic),
                    header + kMagicOffset);
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

SingleFileTarReader::Result TarExtractor::ReadTarFile(char* data,
                                                      uint32_t* size,
                                                      std::string* error_id) {
  const int bytes_read = infile_.ReadAtCurrentPos(data, *size);
  if (bytes_read < 0) {
    *error_id = error::kUnzipGenericError;
    return SingleFileTarReader::Result::kFailure;
  }
  *size = bytes_read;
  return SingleFileTarReader::Result::kSuccess;
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
  if (tar_reader_.ExtractChunk() != SingleFileTarReader::Result::kSuccess) {
    std::move(properties_.failure_callback).Run(tar_reader_.error_id());
    delete this;
    return;
  }

  if (tar_reader_.IsComplete()) {
    std::move(properties_.complete_callback).Run();
    delete this;
    return;
  }

  properties_.progress_callback.Run(tar_reader_.total_bytes().value(),
                                    tar_reader_.curr_bytes());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(&TarExtractor::ExtractChunk,
                                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace image_writer
}  // namespace extensions
