// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/unzip_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "third_party/zlib/google/zip_reader.h"

namespace extensions {
namespace image_writer {

UnzipHelper::UnzipHelper(
    scoped_refptr<base::SequencedTaskRunner> owner_task_runner,
    const base::Callback<void(const base::FilePath&)>& open_callback,
    const base::Closure& complete_callback,
    const base::Callback<void(const std::string&)>& failure_callback,
    const base::Callback<void(int64_t, int64_t)>& progress_callback)
    : owner_task_runner_(owner_task_runner),
      open_callback_(open_callback),
      complete_callback_(complete_callback),
      failure_callback_(failure_callback),
      progress_callback_(progress_callback),
      zip_reader_(std::make_unique<zip::ZipReader>()) {}

UnzipHelper::~UnzipHelper() {}

void UnzipHelper::Unzip(const base::FilePath& image_path,
                        const base::FilePath& temp_dir_path) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunner({base::ThreadPool(), base::MayBlock(),
                                          base::TaskPriority::USER_VISIBLE});
  task_runner->PostTask(FROM_HERE, base::BindOnce(&UnzipHelper::UnzipImpl, this,
                                                  image_path, temp_dir_path));
}

void UnzipHelper::UnzipImpl(const base::FilePath& image_path,
                            const base::FilePath& temp_dir_path) {
  if (!zip_reader_->Open(image_path) || !zip_reader_->AdvanceToNextEntry() ||
      !zip_reader_->OpenCurrentEntryInZip()) {
    OnError(error::kUnzipGenericError);
    return;
  }

  if (zip_reader_->HasMore()) {
    OnError(error::kUnzipInvalidArchive);
    return;
  }

  // Create a new target to unzip to.  The original file is opened by
  // |zip_reader_|.
  zip::ZipReader::EntryInfo* entry_info = zip_reader_->current_entry_info();

  if (!entry_info) {
    OnError(error::kTempDirError);
    return;
  }

  base::FilePath out_image_path =
      temp_dir_path.Append(entry_info->file_path().BaseName());
  OnOpenSuccess(out_image_path);

  zip_reader_->ExtractCurrentEntryToFilePathAsync(
      out_image_path, base::Bind(&UnzipHelper::OnComplete, this),
      base::Bind(&UnzipHelper::OnError, this, error::kUnzipGenericError),
      base::Bind(&UnzipHelper::OnProgress, this, entry_info->original_size()));
}

void UnzipHelper::OnError(const std::string& error) {
  owner_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(failure_callback_, error));
}

void UnzipHelper::OnOpenSuccess(const base::FilePath& image_path) {
  owner_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(open_callback_, image_path));
}

void UnzipHelper::OnComplete() {
  owner_task_runner_->PostTask(FROM_HERE, complete_callback_);
}

void UnzipHelper::OnProgress(int64_t total_bytes, int64_t curr_bytes) {
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(progress_callback_, total_bytes, curr_bytes));
}

}  // namespace image_writer
}  // namespace extensions
