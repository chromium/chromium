// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/image_writer_private/zip_extractor.h"

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"

namespace extensions {
namespace image_writer {

namespace {

// https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
constexpr char kExpectedMagic[4] = {'P', 'K', 0x03, 0x04};

}  // namespace

// static
bool ZipExtractor::IsZipFile(const base::FilePath& image_path) {
  base::File infile(image_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                    base::File::FLAG_WIN_SHARE_DELETE);
  if (!infile.IsValid())
    return false;

  constexpr size_t kExpectedSize = sizeof(kExpectedMagic);
  char actual_magic[kExpectedSize] = {};
  if (infile.ReadAtCurrentPos(actual_magic, kExpectedSize) != kExpectedSize)
    return false;

  return std::equal(kExpectedMagic, kExpectedMagic + kExpectedSize,
                    actual_magic);
}

// static
void ZipExtractor::Extract(ExtractionProperties properties) {
  // ZipExtractor manages its own lifetime, and will delete itself when it
  // completes.
  ZipExtractor* extractor = new ZipExtractor(std::move(properties));
  extractor->ExtractImpl();
}

ZipExtractor::ZipExtractor(ExtractionProperties properties)
    : properties_(std::move(properties)) {}

ZipExtractor::~ZipExtractor() = default;

void ZipExtractor::ExtractImpl() {
  if (!zip_reader_.Open(properties_.image_path)) {
    // |this| will be deleted inside.
    OnError(error::kUnzipGenericError);
    return;
  }

  // If the ZIP can be opened, it shouldn't be empty.
  DCHECK_GT(zip_reader_.num_entries(), 0);

  if (zip_reader_.num_entries() != 1) {
    // |this| will be deleted inside.
    OnError(error::kUnzipInvalidArchive);
    return;
  }

  // Create a new target to unzip to.  The original file is opened by
  // |zip_reader_|.
  const zip::ZipReader::Entry* const entry = zip_reader_.Next();

  if (!entry) {
    // |this| will be deleted inside.
    OnError(error::kTempDirError);
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(entry->path.BaseName());
  std::move(properties_.open_callback).Run(out_image_path);

  // |this| will be deleted when OnComplete or OnError is called.
  zip_reader_.ExtractCurrentEntryToFilePathAsync(
      out_image_path,
      base::BindOnce(&ZipExtractor::OnComplete, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ZipExtractor::OnError, weak_ptr_factory_.GetWeakPtr(),
                     error::kUnzipGenericError),
      base::BindRepeating(properties_.progress_callback, entry->original_size));
}

void ZipExtractor::OnError(const std::string& error) {
  std::move(properties_.failure_callback).Run(error);
  delete this;
}

void ZipExtractor::OnComplete() {
  std::move(properties_.complete_callback).Run();
  delete this;
}

}  // namespace image_writer
}  // namespace extensions
