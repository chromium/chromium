// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/supported_audio_video_checker.h"

#include <stddef.h>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_audio_video_checker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/mime_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

class SupportedAudioVideoExtensions {
 public:
  SupportedAudioVideoExtensions() {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("audio/*", &extensions);
    net::GetExtensionsForMimeType("video/*", &extensions);
    for (size_t i = 0; i < extensions.size(); ++i) {
      std::string mime_type;
      if (net::GetWellKnownMimeTypeFromExtension(extensions[i], &mime_type) &&
          blink::IsSupportedMimeType(mime_type)) {
        audio_video_extensions_.insert(
            base::FilePath::kExtensionSeparator + extensions[i]);
      }
    }
  }

  bool HasSupportedAudioVideoExtension(const base::FilePath& file) {
    return base::ContainsKey(audio_video_extensions_, file.Extension());
  }

 private:
  std::set<base::FilePath::StringType> audio_video_extensions_;

  DISALLOW_COPY_AND_ASSIGN(SupportedAudioVideoExtensions);
};

base::LazyInstance<SupportedAudioVideoExtensions>::DestructorAtExit
    g_audio_video_extensions = LAZY_INSTANCE_INITIALIZER;

base::File OpenBlocking(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

}  // namespace

SupportedAudioVideoChecker::~SupportedAudioVideoChecker() {}

// static
bool SupportedAudioVideoChecker::SupportsFileType(const base::FilePath& path) {
  return g_audio_video_extensions.Get().HasSupportedAudioVideoExtension(path);
}

void SupportedAudioVideoChecker::StartPreWriteValidation(
    const storage::CopyOrMoveFileValidator::ResultCallback& result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(callback_.is_null());
  callback_ = result_callback;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&SupportedAudioVideoChecker::RetrieveConnectorOnUIThread,
                     weak_factory_.GetWeakPtr()));
}

SupportedAudioVideoChecker::SupportedAudioVideoChecker(
    const base::FilePath& path)
    : path_(path),
      weak_factory_(this) {
}

// static
void SupportedAudioVideoChecker::RetrieveConnectorOnUIThread(
    base::WeakPtr<SupportedAudioVideoChecker> this_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<service_manager::Connector> connector =
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->Clone();
  // We need a fresh connector so that we can use it on the IO thread. It has
  // to be retrieved from the UI thread. We must use static method and pass a
  // WeakPtr around as WeakPtrs are not thread-safe.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&SupportedAudioVideoChecker::OnConnectorRetrieved,
                     this_ptr, std::move(connector)));
}

// static
void SupportedAudioVideoChecker::OnConnectorRetrieved(
    base::WeakPtr<SupportedAudioVideoChecker> this_ptr,
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!this_ptr)
    return;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&OpenBlocking, this_ptr->path_),
      base::BindOnce(&SupportedAudioVideoChecker::OnFileOpen, this_ptr,
                     std::move(connector)));
}

void SupportedAudioVideoChecker::OnFileOpen(
    std::unique_ptr<service_manager::Connector> connector,
    base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!file.IsValid()) {
    callback_.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  safe_checker_ = std::make_unique<SafeAudioVideoChecker>(
      std::move(file), callback_, std::move(connector));
  safe_checker_->Start();
}
