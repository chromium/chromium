// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_archive_publisher_impl.h"

#include <errno.h>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/offline_pages/android/offline_page_bridge.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_store_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/OfflinePageArchivePublisherBridge_jni.h"

namespace offline_pages {

namespace {

using base::android::ScopedJavaLocalRef;
using offline_pages::SavePageResult;

// Creates a singleton Delegate.
OfflinePageArchivePublisherImpl::Delegate* GetDefaultDelegate() {
  static OfflinePageArchivePublisherImpl::Delegate delegate;
  return &delegate;
}

bool ShouldUseDownloadsCollection() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_Q;
}

// Helper function to do the move and register synchronously. Make sure this is
// called from a background thread.
PublishArchiveResult MoveAndRegisterArchive(
    const offline_pages::OfflinePageItem& offline_page,
    const base::FilePath& publish_directory,
    OfflinePageArchivePublisherImpl::Delegate* delegate) {
  // For Android Q+, use the downloads collection rather than DownloadManager.
  if (ShouldUseDownloadsCollection()) {
    return delegate->AddCompletedDownload(offline_page);
  }

  OfflinePageItem published_page(offline_page);

  // Calculate the new file name.
  published_page.file_path =
      offline_pages::model_utils::GenerateUniqueFilenameForOfflinePage(
          offline_page.title, offline_page.url, publish_directory);

  // Create the destination directory if it does not already exist.
  if (!publish_directory.empty() && !base::DirectoryExists(publish_directory)) {
    base::File::Error file_error;
    base::CreateDirectoryAndGetError(publish_directory, &file_error);
  }

  // Move the file.
  bool moved = base::Move(offline_page.file_path, published_page.file_path);
  if (!moved) {
    DVPLOG(0) << "OfflinePage publishing file move failure " << __func__;

    if (!base::PathExists(offline_page.file_path)) {
      DVLOG(0) << "Can't copy from non-existent path, from "
               << offline_page.file_path << " " << __func__;
    }
    if (!base::PathExists(publish_directory)) {
      DVLOG(0) << "Target directory does not exist, " << publish_directory
               << " " << __func__;
    }
    return PublishArchiveResult::Failure(SavePageResult::FILE_MOVE_FAILED);
  }

  // Tell the download manager about our file, get back an id.
  if (!delegate->IsDownloadManagerInstalled()) {
    return PublishArchiveResult::Failure(
        SavePageResult::ADD_TO_DOWNLOAD_MANAGER_FAILED);
  }

  return delegate->AddCompletedDownload(published_page);
}

}  // namespace

// static
PublishArchiveResult PublishArchiveResult::Failure(
    SavePageResult save_page_result) {
  return {save_page_result, PublishedArchiveId()};
}

OfflinePageArchivePublisherImpl::OfflinePageArchivePublisherImpl(
    ArchiveManager* archive_manager)
    : archive_manager_(archive_manager), delegate_(GetDefaultDelegate()) {}

OfflinePageArchivePublisherImpl::~OfflinePageArchivePublisherImpl() {}

void OfflinePageArchivePublisherImpl::SetDelegateForTesting(
    OfflinePageArchivePublisherImpl::Delegate* delegate) {
  delegate_ = delegate;
}

void OfflinePageArchivePublisherImpl::PublishArchive(
    const OfflinePageItem& offline_page,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    PublishArchiveDoneCallback publish_done_callback) const {
  background_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MoveAndRegisterArchive, offline_page,
                     archive_manager_->GetPublicArchivesDir(), delegate_),
      base::BindOnce(std::move(publish_done_callback), offline_page));
}

void OfflinePageArchivePublisherImpl::UnpublishArchives(
    const std::vector<PublishedArchiveId>& publish_ids) const {
  std::vector<int64_t> download_manager_ids;

  for (auto& id : publish_ids) {
    if (id.download_id == kArchivePublishedWithoutDownloadId) {
      DCHECK(id.new_file_path.IsContentUri());
      base::DeleteFile(id.new_file_path);
    } else if (id.download_id != kArchiveNotPublished) {
      download_manager_ids.push_back(id.download_id);
    }
  }

  delegate_->Remove(download_manager_ids);
}

// Delegate implementation using Android download manager.

bool OfflinePageArchivePublisherImpl::Delegate::IsDownloadManagerInstalled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jboolean is_installed =
      Java_OfflinePageArchivePublisherBridge_isAndroidDownloadManagerInstalled(
          env);
  return is_installed;
}

PublishArchiveResult
OfflinePageArchivePublisherImpl::Delegate::AddCompletedDownload(
    const OfflinePageItem& page) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (ShouldUseDownloadsCollection()) {
    base::FilePath new_file_path =
        base::FilePath(base::android::ConvertJavaStringToUTF8(
            Java_OfflinePageArchivePublisherBridge_publishArchiveToDownloadsCollection(
                env, android::OfflinePageBridge::ConvertToJavaOfflinePage(
                         env, page))));

    if (new_file_path.empty())
      return PublishArchiveResult::Failure(SavePageResult::FILE_MOVE_FAILED);

    return {SavePageResult::SUCCESS,
            {kArchivePublishedWithoutDownloadId, new_file_path}};
  }

  // TODO(petewil): Handle empty page title.
  std::string page_title = base::UTF16ToUTF8(page.title);

  // Convert strings to jstring references.
  ScopedJavaLocalRef<jstring> j_title =
      base::android::ConvertUTF8ToJavaString(env, page_title);
  // We use the title for a description, since the add to the download manager
  // fails without a description, and we don't have anything better to use.
  ScopedJavaLocalRef<jstring> j_description =
      base::android::ConvertUTF8ToJavaString(env, page_title);
  ScopedJavaLocalRef<jstring> j_path = base::android::ConvertUTF8ToJavaString(
      env, offline_pages::store_utils::ToDatabaseFilePath(page.file_path));
  ScopedJavaLocalRef<jstring> j_uri =
      base::android::ConvertUTF8ToJavaString(env, page.url.spec());
  ScopedJavaLocalRef<jstring> j_referer =
      base::android::ConvertUTF8ToJavaString(env, std::string());

  int64_t download_id =
      Java_OfflinePageArchivePublisherBridge_addCompletedDownload(
          env, j_title, j_description, j_path, page.file_size, j_uri,
          j_referer);
  DCHECK_NE(download_id, kArchivePublishedWithoutDownloadId);
  if (download_id == kArchiveNotPublished)
    return PublishArchiveResult::Failure(
        SavePageResult::ADD_TO_DOWNLOAD_MANAGER_FAILED);

  return {SavePageResult::SUCCESS, {download_id, page.file_path}};
}

int OfflinePageArchivePublisherImpl::Delegate::Remove(
    const std::vector<int64_t>& android_download_manager_ids) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Build a JNI array with our ID data.
  ScopedJavaLocalRef<jlongArray> j_ids =
      base::android::ToJavaLongArray(env, android_download_manager_ids);

  return Java_OfflinePageArchivePublisherBridge_remove(env, j_ids);
}

base::WeakPtr<OfflinePageArchivePublisher>
OfflinePageArchivePublisherImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace offline_pages
