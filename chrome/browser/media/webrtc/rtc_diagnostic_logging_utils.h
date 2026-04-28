// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_RTC_DIAGNOSTIC_LOGGING_UTILS_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_RTC_DIAGNOSTIC_LOGGING_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace rtc_diagnostic_logging {

// Starts RTC diagnostic logging. After calling this function, WebRTC activity
// is subject to logging, but there are no guarantees that logging will actually
// happen as it is subject to authorization and other checks. If
// `should_upload_on_stop` is true, a best-effort attempt will be made to upload
// the log after logging is completed. The callback is invoked with a UUID that
// identifies the log.
void StartRtcDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    bool should_upload_on_stop,
    const base::flat_map<std::string, std::string>& metadata,
    base::OnceCallback<void(const std::string&)> callback);

// Finishes RTC diagnostic logging. If a logging session was started,
// it will end. After `callback` is invoked, the caller can assume no further
// logging will occur. A best-effort attempt will be made to store the log in
// the filesystem. If `should_upload_on_stop` was true when
// StartRtcDiagnosticLogging was called, a best-effort attempt will be made to
// upload the log.
void FinishRtcDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    const base::flat_map<std::string, std::string>& metadata,
    base::OnceClosure callback);

// Cancels RTC diagnostic logging. If a logging session was ongoing,
// it will end and any logs will be discarded and not uploaded. The caller can
// assume that once `callback` is invoked, no further logging will occur.
void CancelRtcDiagnosticLogging(content::RenderFrameHost& frame_host,
                                base::OnceClosure callback);

// Starts RTC peer connection diagnostic logging.
void StartRtcPeerConnectionEventDiagnosticLogging(
    content::RenderFrameHost& frame_host,
    const std::string& session_id,
    base::OnceClosure callback);

}  // namespace rtc_diagnostic_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_RTC_DIAGNOSTIC_LOGGING_UTILS_H_
