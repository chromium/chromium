// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/feed_reliability_logging_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "components/feed/core/v2/public/types.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/status/status.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feed/android/jni_headers/FeedReliabilityLoggingBridge_jni.h"

using base::android::ScopedJavaLocalRef;

namespace feed {
namespace android {
namespace {

jlong ConvertTimestamp(base::TimeTicks ticks) {
  return ticks.since_origin().InNanoseconds();
}

// Note: we're using `absl::StatusCode` for logging network request status
// because it is kept in sync with the proto enum `google.rpc.Code`, which is
// the format the server expects and which is less convenient to use here than
// `absl::StatusCode`.
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/abseil-cpp/absl/status/status.h;drc=083488f5118f2ecf1e927925d9b679f21f8541d0;l=83
// https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto#L32
absl::StatusCode NetErrorToCanonicalStatus(net::Error net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_CLOSED:
      return absl::StatusCode::kUnavailable;
    case net::ERR_NETWORK_CHANGED:
    case net::ERR_CONNECTION_RESET:
      return absl::StatusCode::kAborted;
    case net::ERR_TIMED_OUT:
    case net::ERR_CONNECTION_TIMED_OUT:
      return absl::StatusCode::kDeadlineExceeded;
    case net::ERR_QUIC_PROTOCOL_ERROR:
    default:
      return absl::StatusCode::kInternal;
  }
}

absl::StatusCode HttpStatusToCanonicalStatus(int http_status) {
  switch (http_status) {
    case net::HTTP_OK:
      return absl::StatusCode::kOk;
    case net::HTTP_BAD_REQUEST:
      return absl::StatusCode::kInvalidArgument;
    case net::HTTP_FORBIDDEN:
      return absl::StatusCode::kPermissionDenied;
    case net::HTTP_NOT_FOUND:
      return absl::StatusCode::kNotFound;
    case net::HTTP_CONFLICT:
      return absl::StatusCode::kAlreadyExists;
    case net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
      return absl::StatusCode::kOutOfRange;
    case net::HTTP_TOO_MANY_REQUESTS:
      return absl::StatusCode::kResourceExhausted;
    case 499:  // Client closed request.
      return absl::StatusCode::kCancelled;
    case net::HTTP_NOT_IMPLEMENTED:
      return absl::StatusCode::kUnimplemented;
    case net::HTTP_SERVICE_UNAVAILABLE:
      return absl::StatusCode::kUnavailable;
    case net::HTTP_GATEWAY_TIMEOUT:
      return absl::StatusCode::kDeadlineExceeded;
    case net::HTTP_UNAUTHORIZED:
      return absl::StatusCode::kUnauthenticated;
  }
  if (http_status >= 200 && http_status < 300) {
    return absl::StatusCode::kOk;
  } else if (http_status >= 400 && http_status < 500) {
    return absl::StatusCode::kFailedPrecondition;
  } else if (http_status >= 500 && http_status < 600) {
    return absl::StatusCode::kInternal;
  }
  return absl::StatusCode::kUnknown;
}

int CombinedNetworkStatusCodeToCanonicalStatus(
    int combined_network_status_code) {
  absl::StatusCode canonical_code;
  if (combined_network_status_code >= 0) {
    canonical_code = HttpStatusToCanonicalStatus(combined_network_status_code);
  } else {
    canonical_code = NetErrorToCanonicalStatus(
        static_cast<net::Error>(combined_network_status_code));
  }
  return static_cast<int>(canonical_code);
}

}  // namespace

static jlong JNI_FeedReliabilityLoggingBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_this) {
  return reinterpret_cast<intptr_t>(new FeedReliabilityLoggingBridge(j_this));
}

FeedReliabilityLoggingBridge::FeedReliabilityLoggingBridge(
    const base::android::JavaRef<jobject>& j_this)
    : java_ref_(j_this) {}

FeedReliabilityLoggingBridge::~FeedReliabilityLoggingBridge() = default;

void FeedReliabilityLoggingBridge::LogFeedLaunchOtherStart(
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logOtherLaunchStart(
      base::android::AttachCurrentThread(), java_ref_,
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogCacheReadStart(
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logCacheReadStart(
      base::android::AttachCurrentThread(), java_ref_,
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogCacheReadEnd(
    base::TimeTicks timestamp,
    feedwire::DiscoverCardReadCacheResult result) {
  Java_FeedReliabilityLoggingBridge_logCacheReadEnd(
      base::android::AttachCurrentThread(), java_ref_,
      ConvertTimestamp(timestamp), result);
}

void FeedReliabilityLoggingBridge::LogFeedRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logFeedRequestStart(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogActionsUploadRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logActionsUploadRequestStart(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogWebFeedRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logWebFeedRequestStart(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogSingleWebFeedRequestStart(
    NetworkRequestId id,
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logSingleWebFeedRequestStart(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogRequestSent(NetworkRequestId id,
                                                  base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logRequestSent(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogResponseReceived(
    NetworkRequestId id,
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns,
    base::TimeTicks client_receive_timestamp) {
  Java_FeedReliabilityLoggingBridge_logResponseReceived(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      server_receive_timestamp_ns, server_send_timestamp_ns,
      ConvertTimestamp(client_receive_timestamp));
}

void FeedReliabilityLoggingBridge::LogRequestFinished(
    NetworkRequestId id,
    base::TimeTicks timestamp,
    int combined_network_status_code) {
  Java_FeedReliabilityLoggingBridge_logRequestFinished(
      base::android::AttachCurrentThread(), java_ref_, id.GetUnsafeValue(),
      ConvertTimestamp(timestamp),
      CombinedNetworkStatusCodeToCanonicalStatus(combined_network_status_code));
}

void FeedReliabilityLoggingBridge::LogLoadingIndicatorShown(
    base::TimeTicks timestamp) {
  Java_FeedReliabilityLoggingBridge_logLoadingIndicatorShown(
      base::android::AttachCurrentThread(), java_ref_,
      ConvertTimestamp(timestamp));
}

void FeedReliabilityLoggingBridge::LogAboveTheFoldRender(
    base::TimeTicks timestamp,
    feedwire::DiscoverAboveTheFoldRenderResult result) {
  Java_FeedReliabilityLoggingBridge_logAboveTheFoldRender(
      base::android::AttachCurrentThread(), java_ref_,
      ConvertTimestamp(timestamp), result);
}

void FeedReliabilityLoggingBridge::LogLaunchFinishedAfterStreamUpdate(
    feedwire::DiscoverLaunchResult result) {
  Java_FeedReliabilityLoggingBridge_logLaunchFinishedAfterStreamUpdate(
      base::android::AttachCurrentThread(), java_ref_, result);
}

void FeedReliabilityLoggingBridge::LogLoadMoreStarted() {
  Java_FeedReliabilityLoggingBridge_logLoadMoreStarted(
      base::android::AttachCurrentThread(), java_ref_);
}

void FeedReliabilityLoggingBridge::LogLoadMoreActionUploadRequestStarted() {
  Java_FeedReliabilityLoggingBridge_logLoadMoreActionUploadRequestStarted(
      base::android::AttachCurrentThread(), java_ref_);
}

void FeedReliabilityLoggingBridge::LogLoadMoreRequestSent() {
  Java_FeedReliabilityLoggingBridge_logLoadMoreRequestSent(
      base::android::AttachCurrentThread(), java_ref_);
}

void FeedReliabilityLoggingBridge::LogLoadMoreResponseReceived(
    int64_t server_receive_timestamp_ns,
    int64_t server_send_timestamp_ns) {
  Java_FeedReliabilityLoggingBridge_logLoadMoreResponseReceived(
      base::android::AttachCurrentThread(), java_ref_,
      server_receive_timestamp_ns, server_send_timestamp_ns);
}

void FeedReliabilityLoggingBridge::LogLoadMoreRequestFinished(
    int combined_network_status_code) {
  Java_FeedReliabilityLoggingBridge_logLoadMoreRequestFinished(
      base::android::AttachCurrentThread(), java_ref_,
      CombinedNetworkStatusCodeToCanonicalStatus(combined_network_status_code));
}

void FeedReliabilityLoggingBridge::LogLoadMoreEnded(bool success) {
  Java_FeedReliabilityLoggingBridge_logLoadMoreEnded(
      base::android::AttachCurrentThread(), java_ref_, success);
}

void FeedReliabilityLoggingBridge::ReportExperiments(
    const std::vector<int32_t>& experiment_ids) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedReliabilityLoggingBridge_reportExperiments(env, java_ref_,
                                                      experiment_ids);
}

void FeedReliabilityLoggingBridge::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace android
}  // namespace feed
