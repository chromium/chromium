// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/trace_event/trace_buffer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"

namespace base {
namespace trace_event {

TraceResultBuffer::OutputCallback
TraceResultBuffer::SimpleOutput::GetCallback() {
  return BindRepeating(&SimpleOutput::Append, Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {}

TraceResultBuffer::~TraceResultBuffer() = default;

void TraceResultBuffer::SetOutputCallback(OutputCallback json_chunk_callback) {
  output_callback_ = std::move(json_chunk_callback);
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_) {
    output_callback_.Run(",");
  }
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
}

}  // namespace trace_event
}  // namespace base
