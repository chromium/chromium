// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/task_trace.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "base/pending_task.h"
#include "base/ranges/algorithm.h"
#include "base/task/common/task_annotator.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/log.h>

#include "base/no_destructor.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
namespace debug {
namespace {
#if BUILDFLAG(IS_ANDROID)
// Android sends stdout and stderr to /dev/null; logging should be done through
// the __android_log_write() function. Here we create an override of
// std::stringbuf that writes to the Android log.
class AndroidErrBuffer : public std::stringbuf {
 protected:
  int sync() override {
    __android_log_write(ANDROID_LOG_ERROR, "chromium", str().c_str());
    return 0;
  }
};

std::ostream& DefaultOutputStream() {
  static NoDestructor<AndroidErrBuffer> buf;
  static NoDestructor<std::ostream> out(buf.get());
  return *out;
}
#else
// Use stderr by default.
std::ostream& DefaultOutputStream() {
  return std::cerr;
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace

TaskTrace::TaskTrace() {
  const PendingTask* current_task = TaskAnnotator::CurrentTaskForThread();
  if (!current_task)
    return;
  std::array<const void*, PendingTask::kTaskBacktraceLength + 1> task_trace;
  task_trace[0] = current_task->posted_from.program_counter();
  ranges::copy(current_task->task_backtrace, task_trace.begin() + 1);
  size_t length = 0;
  while (length < task_trace.size() && task_trace[length])
    ++length;
  if (length == 0)
    return;
  stack_trace_.emplace(span(task_trace).first(length));
  trace_overflow_ = current_task->task_backtrace_overflow;
}

bool TaskTrace::empty() const {
  return !stack_trace_.has_value();
}

void TaskTrace::Print() const {
  OutputToStream(&DefaultOutputStream());
}

void TaskTrace::OutputToStream(std::ostream* os) const {
  *os << "Task trace:" << std::endl;
  if (!stack_trace_) {
    *os << "No active task.";
    return;
  }
  *os << *stack_trace_;
  if (trace_overflow_) {
    *os << "Task trace buffer limit hit, update "
           "PendingTask::kTaskBacktraceLength to increase."
        << std::endl;
  }
}

std::string TaskTrace::ToString() const {
  std::stringstream stream;
  OutputToStream(&stream);
  return stream.str();
}

size_t TaskTrace::GetAddresses(span<const void*> addresses) const {
  size_t count = 0;
  if (empty()) {
    return count;
  }
  span<const void* const> current_addresses = stack_trace_->addresses();
  ranges::copy_n(current_addresses.begin(),
                 std::min(current_addresses.size(), addresses.size()),
                 addresses.begin());
  return current_addresses.size();
}

std::ostream& operator<<(std::ostream& os, const TaskTrace& task_trace) {
  task_trace.OutputToStream(&os);
  return os;
}

}  // namespace debug
}  // namespace base
