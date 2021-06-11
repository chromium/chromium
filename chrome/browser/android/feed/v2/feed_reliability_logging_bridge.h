// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_RELIABILITY_LOGGING_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_RELIABILITY_LOGGING_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/public/reliability_logger.h"

namespace feed {
namespace android {

class FeedReliabilityLoggingBridge : public ::feed::ReliabilityLogger {
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
  void SendPendingLaunchEvents(StreamType stream_type,
                               SurfaceId stream_id) override;
  void CancelPendingLaunchEvents() override;
  void LogCacheReadStart(base::TimeTicks timestamp) override;
  void LogCacheReadEnd(
      base::TimeTicks timestamp,
      feedreliabilitylogging::DiscoverCardReadCacheResult result) override;
  int LogFeedRequestStart(base::TimeTicks timestamp) override;
  int LogActionsUploadRequestStart(base::TimeTicks timestamp) override;
  void LogRequestSent(int request_id, base::TimeTicks timestamp) override;
  void LogResponseReceived(int request_id,
                           base::TimeTicks server_receive_timestamp,
                           base::TimeTicks server_send_timestamp,
                           base::TimeTicks client_receive_timestamp) override;
  void LogRequestFinished(int request_id,
                          base::TimeTicks timestamp,
                          int combined_network_status_code) override;
  void LogAtfRenderStart(base::TimeTicks timestamp) override;
  void LogAtfRenderEnd(
      base::TimeTicks timestamp,
      feedreliabilitylogging::DiscoverAboveTheFoldRenderResult result) override;
  void LogLaunchFinished(
      base::TimeTicks timestamp,
      feedreliabilitylogging::DiscoverLaunchResult result) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_RELIABILITY_LOGGING_BRIDGE_H_
