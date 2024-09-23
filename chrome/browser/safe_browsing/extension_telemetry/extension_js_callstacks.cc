// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"

#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/hash/sha1.h"
#include "base/strings/utf_string_conversions.h"

namespace {
constexpr unsigned int kMaxCallStacks = 3;
}

namespace safe_browsing {

ExtensionJSCallStacks::ExtensionJSCallStacks() = default;
ExtensionJSCallStacks::~ExtensionJSCallStacks() = default;
ExtensionJSCallStacks::ExtensionJSCallStacks(const ExtensionJSCallStacks& src) =
    default;

void ExtensionJSCallStacks::Add(const extensions::StackTrace& callstack) {
  if (callstack.empty() || siginfo_callstack_map_.size() == kMaxCallStacks) {
    return;
  }

  SignalInfoJSCallStack siginfo_callstack = ToSignalInfoJSCallStack(callstack);
  std::string key = GetUniqueId(siginfo_callstack);
  siginfo_callstack_map_.emplace(std::move(key), std::move(siginfo_callstack));
}

RepeatedPtrField<SignalInfoJSCallStack> ExtensionJSCallStacks::GetAll() {
  RepeatedPtrField<SignalInfoJSCallStack> siginfo_callstacks;
  for (auto& [key, siginfo_callstack] : siginfo_callstack_map_) {
    siginfo_callstacks.Add(std::move(siginfo_callstack));
  }
  return siginfo_callstacks;
}

// static
unsigned int ExtensionJSCallStacks::MaxCallStacks() {
  return kMaxCallStacks;
}

// static
SignalInfoJSCallStack ExtensionJSCallStacks::ToSignalInfoJSCallStack(
    const extensions::StackTrace& callstack) {
  SignalInfoJSCallStack siginfo_stack;
  for (auto& frame : callstack) {
    SignalInfoJSCallStackFrame siginfo_frame;
    siginfo_frame.set_function_name(base::UTF16ToUTF8(frame.function));
    siginfo_frame.set_script_name(base::UTF16ToUTF8(frame.source));
    siginfo_frame.set_line(frame.line_number);
    siginfo_frame.set_column(frame.column_number);
    *siginfo_stack.add_frames() = std::move(siginfo_frame);
  }
  return siginfo_stack;
}

// static
extensions::StackTrace ExtensionJSCallStacks::ToExtensionsStackTrace(
    const SignalInfoJSCallStack& siginfo_callstack) {
  extensions::StackTrace stack_trace;
  for (int i = 0; i < siginfo_callstack.frames_size(); i++) {
    const SignalInfoJSCallStackFrame& siginfo_frame =
        siginfo_callstack.frames(i);
    extensions::StackFrame frame;
    frame.line_number = siginfo_frame.line();
    frame.column_number = siginfo_frame.column();
    frame.source = base::UTF8ToUTF16(siginfo_frame.script_name());
    frame.function = base::UTF8ToUTF16(siginfo_frame.function_name());
    stack_trace.push_back(std::move(frame));
  }

  return stack_trace;
}

// static
std::string ExtensionJSCallStacks::GetUniqueId(
    const SignalInfoJSCallStack& siginfo_callstack) {
  std::stringstream ss;
  for (auto& siginfo_frame : siginfo_callstack.frames()) {
    ss << siginfo_frame.function_name() << siginfo_frame.script_name()
       << siginfo_frame.line() << siginfo_frame.column();
  }
  return base::SHA1HashString(ss.str());
}

// Used for debug logging only.
// static
std::string ExtensionJSCallStacks::SignalInfoJSCallStackAsString(
    const SignalInfoJSCallStack& js_callstack) {
  std::stringstream ss;
  for (const auto& frame : js_callstack.frames()) {
    ss << "\n          function: " << frame.function_name();
    ss << "  script: " << frame.script_name();
    ss << "  line: " << frame.line();
    ss << "  col: " << frame.column();
  }
  ss << "\n";
  return ss.str();
}

}  // namespace safe_browsing
