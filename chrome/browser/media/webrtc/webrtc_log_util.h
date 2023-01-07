// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_

class WebRtcLogUtil {
 public:
  // Calls webrtc_logging::DeleteOldWebRtcLogFiles() for all profiles. Must be
  // called on the UI thread.
  static void DeleteOldWebRtcLogFilesForAllProfiles();
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_UTIL_H_
