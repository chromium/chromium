// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_buffer.h"

#include "base/logging.h"

WebRtcLogBuffer::WebRtcLogBuffer()
    : buffer_(),
      circular_(&buffer_[0], sizeof(buffer_), sizeof(buffer_) / 2, false),
      read_only_(false) {}

WebRtcLogBuffer::~WebRtcLogBuffer() {
#if DCHECK_IS_ON()
  DCHECK(read_only_ || sequence_checker_.CalledOnValidSequence());
#endif
}

void WebRtcLogBuffer::Log(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!read_only_);
  circular_.Write(message.c_str(), message.length());
  const char eol = '\n';
  circular_.Write(&eol, 1);
}

webrtc_logging::PartialCircularBuffer WebRtcLogBuffer::Read() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_only_);
  return webrtc_logging::PartialCircularBuffer(&buffer_[0], sizeof(buffer_));
}

void WebRtcLogBuffer::SetComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!read_only_) << "Already set? (programmer error)";
  read_only_ = true;
  // Detach from the current sequence so that we can check reads on a different
  // sequence. This is to make sure that Read()s still happen on one sequence
  // only.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}
