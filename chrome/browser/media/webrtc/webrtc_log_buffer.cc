// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_buffer.h"

#include <ostream>

#include "base/check_op.h"

WebRtcLogBuffer::WebRtcLogBuffer()
    : buffer_(),
      circular_(buffer_, sizeof(buffer_) / 2, false),
      read_only_(false) {}

WebRtcLogBuffer::~WebRtcLogBuffer() {
#if DCHECK_IS_ON()
  CHECK(read_only_ || sequence_checker_.CalledOnValidSequence(),
        base::NotFatalUntil::M161);
#endif
}

void WebRtcLogBuffer::Log(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!read_only_, base::NotFatalUntil::M161);
  circular_.Write(base::as_bytes(base::span(message)));
  const char eol = '\n';
  circular_.Write(base::as_bytes(base::span_from_ref(eol)));
}

webrtc_logging::PartialCircularBuffer WebRtcLogBuffer::Read() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(read_only_, base::NotFatalUntil::M161);
  return webrtc_logging::PartialCircularBuffer(buffer_);
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
