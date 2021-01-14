// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/zip_extractor.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {

// static
bool ZipExtractor::IsZipFile(const base::FilePath& image_path) {
  // TODO(tetsui): Check the file header instead of the extension.
  return image_path.Extension() == FILE_PATH_LITERAL(".zip");
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
  if (!zip_reader_.Open(properties_.image_path) ||
      !zip_reader_.AdvanceToNextEntry() ||
      !zip_reader_.OpenCurrentEntryInZip()) {
    // |this| will be deleted inside.
    OnError(error::kUnzipGenericError);
    return;
  }

  if (zip_reader_.HasMore()) {
    // |this| will be deleted inside.
    OnError(error::kUnzipInvalidArchive);
    return;
  }

  // Create a new target to unzip to.  The original file is opened by
  // |zip_reader_|.
  zip::ZipReader::EntryInfo* entry_info = zip_reader_.current_entry_info();

  if (!entry_info) {
    // |this| will be deleted inside.
    OnError(error::kTempDirError);
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(entry_info->file_path().BaseName());
  std::move(properties_.open_callback).Run(out_image_path);

  // |this| will be deleted when OnComplete or OnError is called.
  zip_reader_.ExtractCurrentEntryToFilePathAsync(
      out_image_path,
      base::BindOnce(&ZipExtractor::OnComplete, weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ZipExtractor::OnError, weak_ptr_factory_.GetWeakPtr(),
                     error::kUnzipGenericError),
      base::BindRepeating(properties_.progress_callback,
                          entry_info->original_size()));
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
