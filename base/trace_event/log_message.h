// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_LOG_MESSAGE_H_
#define BASE_TRACE_EVENT_LOG_MESSAGE_H_

#include <stddef.h>

#include <string>

#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event_impl.h"

namespace base {

namespace trace_event {

class BASE_EXPORT LogMessage : public ConvertableToTraceFormat {
 public:
  LogMessage(const char* file, base::StringPiece message, int line);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  ~LogMessage() override;

  // ConvertableToTraceFormat class implementation.
  void AppendAsTraceFormat(std::string* out) const override;
  bool AppendToProto(ProtoAppender* appender) override;

  void EstimateTraceMemoryOverhead(TraceEventMemoryOverhead* overhead) override;

  const char* file() const { return file_; }
  const std::string& message() const { return message_; }
  int line_number() const { return line_number_; }

 private:
  const char* file_;
  std::string message_;
  int line_number_;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_LOG_MESSAGE_H_
