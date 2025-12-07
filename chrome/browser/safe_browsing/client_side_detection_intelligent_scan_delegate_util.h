// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_UTIL_H_

namespace base {
class TimeTicks;
}

namespace safe_browsing::client_side_detection {

// Logs whether on-device model execution was successful and how long it took.
void LogOnDeviceModelExecutionSuccessAndTime(
    bool success,
    base::TimeTicks session_execution_start_time);

// Logs how long it took to create the on-device model session.
void LogOnDeviceModelSessionCreationTime(
    base::TimeTicks session_creation_start_time);

// Logs how long it took for the on-device model to be downloaded.
void LogOnDeviceModelFetchTime(base::TimeTicks on_device_fetch_time);

// Logs whether the on-device model was successfully downloaded.
void LogOnDeviceModelDownloadSuccess(bool success);

// Logs whether the on-device model session was alive on delegate shutdown.
void LogOnDeviceModelSessionAliveOnDelegateShutdown(bool session_alive);

}  // namespace safe_browsing::client_side_detection

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_INTELLIGENT_SCAN_DELEGATE_UTIL_H_
