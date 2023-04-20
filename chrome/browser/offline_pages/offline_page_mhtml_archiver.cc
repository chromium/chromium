// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_clock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/filename_util.h"

namespace offline_pages {
namespace {

void DeleteFileOnFileThread(const base::FilePath& file_path,
                            base::OnceClosure callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeleteFileCallback(
          file_path, base::OnceCallback<void(bool)>(base::DoNothing())
                         .Then(std::move(callback))));
}

// Compute a SHA256 digest using a background thread. The computed digest will
// be returned in the callback parameter. If it is empty, the digest calculation
// fails.
void ComputeDigestOnFileThread(
    const base::FilePath& file_path,
    base::OnceCallback<void(const std::string&)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ArchiveValidator::ComputeDigest, file_path),
      std::move(callback));
}
}  // namespace

// static
OfflinePageMHTMLArchiver::OfflinePageMHTMLArchiver() {}

OfflinePageMHTMLArchiver::~OfflinePageMHTMLArchiver() {
}

void OfflinePageMHTMLArchiver::CreateArchive(
    const base::FilePath& archives_dir,
    const CreateArchiveParams& create_archive_params,
    content::WebContents* web_contents,
    CreateArchiveCallback callback) {
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());
  callback_ = std::move(callback);

  GenerateMHTML(archives_dir, web_contents, create_archive_params);
}

void OfflinePageMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir,
    content::WebContents* web_contents,
    const CreateArchiveParams& create_archive_params) {
  if (archives_dir.empty()) {
    DVLOG(1) << "Archive path was empty. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  if (!web_contents) {
    DVLOG(1) << "WebContents is missing. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  if (!web_contents->GetRenderViewHost()) {
    DVLOG(1) << "RenderViewHost is not created yet. Can't create archive.";
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  GURL url(web_contents->GetLastCommittedURL());
  std::u16string title(web_contents->GetTitle());
  base::FilePath file_path(
      archives_dir.Append(base::Uuid::GenerateRandomV4().AsLowercaseString())
          .AddExtension(OfflinePageUtils::kMHTMLExtension));
  content::MHTMLGenerationParams params(file_path);
  params.use_binary_encoding = true;
  params.remove_popup_overlay = create_archive_params.remove_popup_overlay;

  web_contents->GenerateMHTMLWithResult(
      params,
      base::BindOnce(&OfflinePageMHTMLArchiver::OnGenerateMHTMLDone,
                     weak_ptr_factory_.GetWeakPtr(), url, file_path, title,
                     create_archive_params.name_space, OfflineTimeNow()));
}

void OfflinePageMHTMLArchiver::OnGenerateMHTMLDone(
    const GURL& url,
    const base::FilePath& file_path,
    const std::u16string& title,
    const std::string& name_space,
    base::Time mhtml_start_time,
    const content::MHTMLGenerationResult& result) {
  if (result.file_size < 0) {
    DeleteFileAndReportFailure(file_path,
                               ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  if (result.file_digest) {
    OnComputeDigestDone(url, file_path, title, name_space, base::Time(),
                        result.file_size, result.file_digest.value());
  } else {
    const base::Time digest_start_time = OfflineTimeNow();
    ComputeDigestOnFileThread(
        file_path,
        base::BindOnce(&OfflinePageMHTMLArchiver::OnComputeDigestDone,
                       weak_ptr_factory_.GetWeakPtr(), url, file_path, title,
                       name_space, digest_start_time, result.file_size));
  }
}

void OfflinePageMHTMLArchiver::OnComputeDigestDone(
    const GURL& url,
    const base::FilePath& file_path,
    const std::u16string& title,
    const std::string& name_space,
    base::Time digest_start_time,
    int64_t file_size,
    const std::string& digest) {
  if (digest.empty()) {
    DeleteFileAndReportFailure(file_path,
                               ArchiverResult::ERROR_DIGEST_CALCULATION_FAILED);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), ArchiverResult::SUCCESSFULLY_CREATED,
                     url, file_path, title, file_size, digest));
}

void OfflinePageMHTMLArchiver::DeleteFileAndReportFailure(
    const base::FilePath& file_path,
    ArchiverResult result) {
  DeleteFileOnFileThread(
      file_path, base::BindOnce(&OfflinePageMHTMLArchiver::ReportFailure,
                                weak_ptr_factory_.GetWeakPtr(), result));
}

void OfflinePageMHTMLArchiver::ReportFailure(ArchiverResult result) {
  DCHECK(result != ArchiverResult::SUCCESSFULLY_CREATED);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), result, GURL(), base::FilePath(),
                     std::u16string(), 0, std::string()));
}

}  // namespace offline_pages
