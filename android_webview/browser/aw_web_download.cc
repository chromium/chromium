// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_download.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwWebDownload_jni.h"

namespace android_webview {

namespace {

using Result = AwWebDownload::Result;

Result ErrorCodeForInterruptReason(download::DownloadInterruptReason reason) {
  switch (reason) {
    case download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
    case download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
      return Result::kErrorCancelled;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
      return Result::kErrorInsufficientSpace;
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE:
      return Result::kErrorFile;
    // HTTP 401 and 407 (SERVER_UNAUTHORIZED) and 403 (SERVER_FORBIDDEN), see
    // download::HandleSuccessfulServerResponse().
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
      return Result::kErrorServerAccessDenied;
    // Any other HTTP error status: 404, 204, 205 or an invalid range response
    // (SERVER_BAD_CONTENT), 416 or an ignored range (SERVER_NO_RANGE), and
    // everything else, such as 410 or 5xx (SERVER_FAILED).
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
      return Result::kErrorServer;
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT:
      return Result::kErrorNetwork;
    // Not expected in WebView, which has no download protection, enterprise
    // policies or download history.
    case download::DOWNLOAD_INTERRUPT_REASON_NONE:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
    case download::DOWNLOAD_INTERRUPT_REASON_CRASH:
    case download::DOWNLOAD_INTERRUPT_REASON_LOCAL_DOWNLOAD_BLOCKED:
      return Result::kErrorUnknown;
  }
  NOTREACHED();
}

// WebView has no download UI or history, so nothing else ever removes a
// download from the DownloadManager. Removing an interrupted download deletes
// its partial file; a completed download keeps its file.
void RemoveDownload(base::WeakPtr<content::BrowserContext> context,
                    const std::string& guid) {
  if (!context) {
    return;
  }
  if (download::DownloadItem* item =
          context->GetDownloadManager()->GetDownloadByGuid(guid)) {
    item->Remove();
  }
}

}  // namespace

AwWebDownload::AwWebDownload(download::DownloadItem* item,
                             download::DownloadTargetCallback target_callback)
    : item_(item), target_callback_(std::move(target_callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  item_->AddObserver(this);
}

AwWebDownload::~AwWebDownload() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Either the download is over, or the embedder dropped it without deciding (a
  // saveTo() in flight keeps the Java peer, and so this object, alive). In the
  // latter case the cancellation reaches OnDownloadUpdated() synchronously,
  // which detaches from the item.
  Cancel();
  CHECK(!item_);
}

void AwWebDownload::SaveTo(const base::android::JavaRef<jobject>& java_download,
                           const std::string& target) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  java_download_.Reset(java_download);
  if (!target_callback_) {
    // Generally unexpected, since AwWebDownload.java rejects saveTo() after
    // cancel(). It can still happen in niche cases where the download ends
    // before the embedder decides, e.g. when it is cancelled from DevTools.
    ReportResult(Result::kErrorCancelled);
    return;
  }

  // Use the destination as both the intermediate and the final path. Bytes
  // received before this point live in a temporary file and are moved into
  // the destination by DownloadFile::RenameAndUniquify(). For content:// URIs,
  // DownloadCollectionBridge handles MediaStore items.
  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(target);
  target_info.intermediate_path = target_info.target_path;
  std::move(target_callback_).Run(std::move(target_info));
}

void AwWebDownload::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (target_callback_) {
    // An empty target path cancels a download whose target is being determined.
    std::move(target_callback_).Run(download::DownloadTargetInfo());
  } else if (item_) {
    item_->Cancel(/*user_cancel=*/true);
  }
}

void AwWebDownload::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(item, item_);
  Result result;
  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      // Also reported while DownloadItemImpl retries an interrupted transfer;
      // INTERRUPTED means it gave up.
      return;
    case download::DownloadItem::COMPLETE:
      result = Result::kSuccess;
      break;
    case download::DownloadItem::CANCELLED:
      result = Result::kErrorCancelled;
      break;
    case download::DownloadItem::INTERRUPTED:
      result = ErrorCodeForInterruptReason(item->GetLastReason());
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
  Detach();
  // Posted, because a DownloadItem must not be removed from within its
  // observers or while its target is being determined.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoveDownload,
          content::DownloadItemUtils::GetBrowserContext(item)->GetWeakPtr(),
          item->GetGuid()));
  ReportResult(result);
}

void AwWebDownload::OnDownloadDestroyed(download::DownloadItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(item, item_);
  Detach();
  ReportResult(Result::kErrorCancelled);
}

void AwWebDownload::Detach() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  item_->RemoveObserver(this);
  item_ = nullptr;
  target_callback_.Reset();
}

void AwWebDownload::ReportResult(Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!java_download_) {
    return;
  }
  // Releasing the reference lets the Java peer be garbage collected.
  // onFinished() destroys this object, so callers must not touch it afterwards.
  base::android::ScopedJavaGlobalRef<jobject> java_download =
      std::move(java_download_);
  Java_AwWebDownload_onFinished(base::android::AttachCurrentThread(),
                                java_download, result);
}

}  // namespace android_webview

DEFINE_JNI(AwWebDownload)
