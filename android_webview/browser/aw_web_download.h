// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_DOWNLOAD_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_DOWNLOAD_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_target_info.h"

namespace android_webview {

// Native peer of the Java AwWebDownload. It holds the pending download target
// decision for a DownloadItem started in a WebView that has a download handler,
// and reports the outcome of AwWebDownload#saveTo() back to Java.
//
// Owned by the Java AwWebDownload through a JniUniquePtr, which deletes it once
// the download is over, or once the Java object is garbage collected. Lives on
// the UI thread. While a saveTo() request is in flight this object keeps the
// Java peer alive, so the request is not dropped if the embedder stops
// referencing the download.
//
// The download ends for this object when the DownloadItem leaves IN_PROGRESS or
// is destroyed: it then stops observing the item, schedules the item's removal
// from the DownloadManager and reports the result of a pending saveTo().
// Afterwards Cancel() has no effect and SaveTo() reports
// Result::kErrorCancelled.
class AwWebDownload final : public download::DownloadItem::Observer {
 public:
  // Outcome of AwWebDownload#saveTo(). The error values are passed through to
  // the AndroidX API, so they must match androidx.webkit.DownloadException.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AwWebDownloadResult
  enum class Result {
    // The download was saved to the destination.
    kSuccess = -1,
    // The download failed for a reason not covered by the other error codes.
    kErrorUnknown = 0,
    // The download was cancelled.
    kErrorCancelled = 1,
    // The network transfer failed, for example because the connection dropped.
    kErrorNetwork = 2,
    // The download could not be written, for example because the destination
    // is not writable.
    kErrorFile = 3,
    // The device ran out of storage space.
    kErrorInsufficientSpace = 4,
    // The server responded with an HTTP error status, such as 404, 410 or 5xx.
    kErrorServer = 5,
    // The server denied access (HTTP 401, 403 or 407), for example because a
    // link expired.
    kErrorServerAccessDenied = 6,
  };

  AwWebDownload(download::DownloadItem* item,
                download::DownloadTargetCallback target_callback);

  AwWebDownload(const AwWebDownload&) = delete;
  AwWebDownload& operator=(const AwWebDownload&) = delete;

  // Cancels the download if the embedder has not decided yet.
  ~AwWebDownload() override;

  // Called from Java on the UI thread.
  // Uses `target` (an absolute file path or a content:// URI) as the download
  // target and reports the outcome to `java_download`.
  void SaveTo(const base::android::JavaRef<jobject>& java_download,
              const std::string& target);
  void Cancel();

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

 private:
  // Stops observing the item and drops the pending target decision.
  void Detach();
  void ReportResult(Result result);

  // Null once the download ended.
  raw_ptr<download::DownloadItem> item_;
  // Null once the embedder decided or the download ended.
  download::DownloadTargetCallback target_callback_;
  // Set while the result of saveTo() is pending.
  base::android::ScopedJavaGlobalRef<jobject> java_download_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_DOWNLOAD_H_
