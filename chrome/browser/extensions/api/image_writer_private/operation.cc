// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <string_view>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/zip_extractor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

namespace {

const int kMD5BufferSize = 1024;

// Returns true if the file at |image_path| is an archived image.
bool IsArchive(const base::FilePath& image_path) {
  return ZipExtractor::IsZipFile(image_path) ||
         TarExtractor::IsTarFile(image_path) ||
         XzExtractor::IsXzFile(image_path);
}

// Extracts the archive at |image_path| using to |temp_dir_path| using a proper
// extractor.
void ExtractArchive(ExtractionProperties properties) {
  if (ZipExtractor::IsZipFile(properties.image_path)) {
    ZipExtractor::Extract(std::move(properties));
  } else if (TarExtractor::IsTarFile(properties.image_path)) {
    TarExtractor::Extract(std::move(properties));
  } else if (XzExtractor::IsXzFile(properties.image_path)) {
    XzExtractor::Extract(std::move(properties));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

Operation::Operation(base::WeakPtr<OperationManager> manager,
                     const ExtensionId& extension_id,
                     const std::string& device_path,
                     const base::FilePath& download_folder)
    : manager_(manager),
      extension_id_(extension_id),
#if BUILDFLAG(IS_WIN)
      device_path_(base::FilePath::FromUTF8Unsafe(device_path)),
#else
      device_path_(device_path),
#endif
      temp_dir_(std::make_unique<base::ScopedTempDir>()),
      stage_(image_writer_api::Stage::kUnknown),
      progress_(0),
      download_folder_(download_folder),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(blocking_task_traits())) {
}

Operation::~Operation() {
  // base::ScopedTempDir must be destroyed on a thread that allows blocking IO
  // because it will try delete the directory if a call to Delete() hasn't been
  // made or was unsuccessful.
  task_runner_->DeleteSoon(FROM_HERE, std::move(temp_dir_));
}

void Operation::Cancel() {
  DCHECK(IsRunningInCorrectSequence());

  stage_ = image_writer_api::Stage::kNone;

  CleanUp();
}

void Operation::Abort() {
  DCHECK(IsRunningInCorrectSequence());
  Error(error::kAborted);
}

int Operation::GetProgress() {
  return progress_;
}

image_writer_api::Stage Operation::GetStage() {
  return stage_;
}

void Operation::PostTask(base::OnceClosure task) {
  task_runner_->PostTask(FROM_HERE, std::move(task));
}

void Operation::Start() {
  DCHECK(IsRunningInCorrectSequence());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (download_folder_.empty() ||
      !temp_dir_->CreateUniqueTempDirUnderPath(download_folder_)) {
#else
  if (!temp_dir_->CreateUniqueTempDir()) {
#endif
    Error(error::kTempDirError);
    return;
  }

  AddCleanUpFunction(
      base::BindOnce(base::IgnoreResult(&base::ScopedTempDir::Delete),
                     base::Unretained(temp_dir_.get())));

  StartImpl();
}

void Operation::OnExtractOpenComplete(const base::FilePath& image_path) {
  DCHECK(IsRunningInCorrectSequence());
  image_path_ = image_path;
}

void Operation::Extract(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  if (IsArchive(image_path_)) {
    SetStage(image_writer_api::Stage::kUnzip);

    ExtractionProperties properties;
    properties.image_path = image_path_;
    properties.temp_dir_path = temp_dir_->GetPath();
    properties.open_callback =
        base::BindOnce(&Operation::OnExtractOpenComplete, this);
    properties.complete_callback = base::BindOnce(
        &Operation::CompleteAndContinue, this, std::move(continuation));
    properties.failure_callback =
        base::BindOnce(&Operation::OnExtractFailure, this);
    properties.progress_callback =
        base::BindRepeating(&Operation::OnExtractProgress, this);

    ExtractArchive(std::move(properties));
  } else {
    PostTask(std::move(continuation));
  }
}

void Operation::Finish() {
  DCHECK(IsRunningInCorrectSequence());

  CleanUp();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OperationManager::OnComplete, manager_, extension_id_));
}

void Operation::Error(const std::string& error_message) {
  DCHECK(IsRunningInCorrectSequence());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OperationManager::OnError, manager_, extension_id_,
                     stage_, progress_, error_message));

  CleanUp();
}

void Operation::SetProgress(int progress) {
  DCHECK(IsRunningInCorrectSequence());

  if (progress <= progress_) {
    return;
  }

  if (IsCancelled()) {
    return;
  }

  progress_ = progress;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OperationManager::OnProgress, manager_,
                                extension_id_, stage_, progress_));
}

void Operation::SetStage(image_writer_api::Stage stage) {
  DCHECK(IsRunningInCorrectSequence());

  if (IsCancelled())
    return;

  stage_ = stage;
  progress_ = 0;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OperationManager::OnProgress, manager_,
                                extension_id_, stage_, progress_));
}

bool Operation::IsCancelled() {
  DCHECK(IsRunningInCorrectSequence());

  return stage_ == image_writer_api::Stage::kNone;
}

void Operation::AddCleanUpFunction(base::OnceClosure callback) {
  DCHECK(IsRunningInCorrectSequence());
  cleanup_functions_.push_back(std::move(callback));
}

void Operation::CompleteAndContinue(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  SetProgress(kProgressComplete);
  PostTask(std::move(continuation));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void Operation::StartUtilityClient() {
  DCHECK(IsRunningInCorrectSequence());
  if (!image_writer_client_.get()) {
    image_writer_client_ = ImageWriterUtilityClient::Create(task_runner_);
    AddCleanUpFunction(base::BindOnce(&Operation::StopUtilityClient, this));
  }
}

void Operation::StopUtilityClient() {
  DCHECK(IsRunningInCorrectSequence());
  image_writer_client_->Shutdown();
}

void Operation::WriteImageProgress(int64_t total_bytes, int64_t curr_bytes) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  int progress = kProgressComplete * curr_bytes / total_bytes;

  if (progress > GetProgress()) {
    SetProgress(progress);
  }
}
#endif

void Operation::GetMD5SumOfFile(
    const base::FilePath& file_path,
    int64_t file_size,
    int progress_offset,
    int progress_scale,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  base::MD5Init(&md5_context_);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    Error(error::kImageOpenError);
    return;
  }

  if (file_size <= 0) {
    file_size = file.GetLength();
    if (file_size < 0) {
      Error(error::kImageOpenError);
      return;
    }
  }

  PostTask(base::BindOnce(&Operation::MD5Chunk, this, std::move(file), 0,
                          file_size, progress_offset, progress_scale,
                          std::move(callback)));
}

bool Operation::IsRunningInCorrectSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

void Operation::MD5Chunk(
    base::File file,
    int64_t bytes_processed,
    int64_t bytes_total,
    int progress_offset,
    int progress_scale,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled())
    return;

  CHECK_LE(bytes_processed, bytes_total);

  auto buffer = base::HeapArray<char>::Uninit(kMD5BufferSize);
  int read_size = std::min(bytes_total - bytes_processed,
                           static_cast<int64_t>(kMD5BufferSize));

  if (read_size == 0) {
    // Nothing to read, we are done.
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context_);
    std::move(callback).Run(base::MD5DigestToBase16(digest));
  } else {
    int len = file.Read(bytes_processed, buffer.data(), read_size);

    if (len == read_size) {
      // Process data.
      base::MD5Update(&md5_context_, std::string_view(buffer.data(), len));
      int percent_curr =
          ((bytes_processed + len) * progress_scale) / bytes_total +
          progress_offset;
      SetProgress(percent_curr);

      PostTask(base::BindOnce(
          &Operation::MD5Chunk, this, std::move(file), bytes_processed + len,
          bytes_total, progress_offset, progress_scale, std::move(callback)));
      // Skip closing the file.
      return;
    } else {
      // We didn't read the bytes we expected.
      Error(error::kHashReadError);
    }
  }
}

void Operation::OnExtractFailure(const std::string& error) {
  DCHECK(IsRunningInCorrectSequence());
  Error(error);
}

void Operation::OnExtractProgress(int64_t total_bytes, int64_t progress_bytes) {
  DCHECK(IsRunningInCorrectSequence());

  int progress_percent = kProgressComplete * progress_bytes / total_bytes;
  SetProgress(progress_percent);
}

void Operation::CleanUp() {
  DCHECK(IsRunningInCorrectSequence());
  for (base::OnceClosure& cleanup_function : cleanup_functions_)
    std::move(cleanup_function).Run();
  cleanup_functions_.clear();
}

}  // namespace image_writer
}  // namespace extensions
