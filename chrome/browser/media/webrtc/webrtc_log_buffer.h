// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_BUFFER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_BUFFER_H_

#include <string>

#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/webrtc_logging/common/partial_circular_buffer.h"

#if BUILDFLAG(IS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

class WebRtcLogBuffer {
 public:
  WebRtcLogBuffer();
  ~WebRtcLogBuffer();

  void Log(const std::string& message);

  // Returns a circular buffer instance for reading the internal log buffer.
  // Must only be called after the log has been marked as complete
  // (see SetComplete) and the caller must ensure that the WebRtcLogBuffer
  // instance remains in scope for the lifetime of the returned circular buffer.
  webrtc_logging::PartialCircularBuffer Read();

  // Switches the buffer to read-only mode, where access to the internal
  // buffer is allowed from different threads than were used to contribute
  // to the log.  Calls to Log() won't be allowed after calling
  // SetComplete() and the call to SetComplete() must be done on the same
  // thread as constructed the buffer and calls Log().
  void SetComplete();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  uint8_t buffer_[kWebRtcLogSize];
  webrtc_logging::PartialCircularBuffer circular_;
  bool read_only_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_LOG_BUFFER_H_
