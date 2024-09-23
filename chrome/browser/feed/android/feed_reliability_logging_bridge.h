// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_FEED_RELIABILITY_LOGGING_BRIDGE_H_
#define CHROME_BROWSER_FEED_ANDROID_FEED_RELIABILITY_LOGGING_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {
namespace android {

class FeedReliabilityLoggingBridge : public ::feed::ReliabilityLoggingBridge {
 public:
  explicit FeedReliabilityLoggingBridge(
      const base::android::JavaRef<jobject>& j_this);
  FeedReliabilityLoggingBridge(const FeedReliabilityLoggingBridge&) = delete;
  FeedReliabilityLoggingBridge& operator=(const FeedReliabilityLoggingBridge&) =
      delete;
  ~FeedReliabilityLoggingBridge() override;

  // Called by Java to delete this object.
  void Destroy(JNIEnv* env);

  // ::feed::ReliabilityLogger implementation.
  void LogFeedLaunchOtherStart(base::TimeTicks timestamp) override;
  void LogCacheReadStart(base::TimeTicks timestamp) override;
  void LogCacheReadEnd(base::TimeTicks timestamp,
                       feedwire::DiscoverCardReadCacheResult result) override;
  void LogFeedRequestStart(NetworkRequestId id,
                           base::TimeTicks timestamp) override;
  void LogActionsUploadRequestStart(NetworkRequestId id,
                                    base::TimeTicks timestamp) override;
  void LogWebFeedRequestStart(NetworkRequestId id,
                              base::TimeTicks timestamp) override;
  void LogSingleWebFeedRequestStart(NetworkRequestId id,
                                    base::TimeTicks timestamp) override;
  void LogRequestSent(NetworkRequestId id, base::TimeTicks timestamp) override;
  void LogResponseReceived(NetworkRequestId id,
                           int64_t server_receive_timestamp_ns,
                           int64_t server_send_timestamp_ns,
                           base::TimeTicks client_receive_timestamp) override;
  void LogRequestFinished(NetworkRequestId id,
                          base::TimeTicks timestamp,
                          int combined_network_status_code) override;
  void LogLoadingIndicatorShown(base::TimeTicks timestamp) override;
  void LogAboveTheFoldRender(
      base::TimeTicks timestamp,
      feedwire::DiscoverAboveTheFoldRenderResult result) override;
  void LogLaunchFinishedAfterStreamUpdate(
      feedwire::DiscoverLaunchResult result) override;
  void LogLoadMoreStarted() override;
  void LogLoadMoreActionUploadRequestStarted() override;
  void LogLoadMoreRequestSent() override;
  void LogLoadMoreResponseReceived(int64_t server_receive_timestamp_ns,
                                   int64_t server_send_timestamp_ns) override;
  void LogLoadMoreRequestFinished(int combined_network_status_code) override;
  void LogLoadMoreEnded(bool success) override;
  void ReportExperiments(const std::vector<int32_t>& experiment_ids) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_FEED_RELIABILITY_LOGGING_BRIDGE_H_
