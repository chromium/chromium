// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_JS_CALLSTACKS_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_JS_CALLSTACKS_H_

#include "base/containers/flat_map.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/stack_frame.h"

namespace safe_browsing {

using ::google::protobuf::RepeatedPtrField;
using SignalInfoJSCallStackFrame =
    ExtensionTelemetryReportRequest::SignalInfo::JSCallStackFrame;
using SignalInfoJSCallStack =
    ExtensionTelemetryReportRequest::SignalInfo::JSCallStack;

// This is a helper class used to store and retrieve JS callstacks
// for extension API signals. A signal processor class, for e.g.
// TabsApiSignalProcessor, includes this class as a member to store
// callstacks for corresponding (for e.g, chrome.tabs) API calls.
class ExtensionJSCallStacks {
 public:
  ExtensionJSCallStacks();
  ~ExtensionJSCallStacks();
  ExtensionJSCallStacks(const ExtensionJSCallStacks&);

  // Stores the input `callstack`. Does nothing if:
  //   - the callstack being added is empty
  //   - max number of callstacks already stored
  //   - the callstack is a duplicate of one already stored
  //
  void Add(const extensions::StackTrace& callstack);

  // Returns all the callstacks stored in the SignalInfo protobuf format.
  RepeatedPtrField<SignalInfoJSCallStack> GetAll();

  // Returns the current number of callstacks stored.
  unsigned int NumCallStacks() { return siginfo_callstack_map_.size(); }

  // Returns the maximum number of distinct callstacks that
  // will be stored. Attempts to add more will be ignored.
  static unsigned int MaxCallStacks();

  // Helpers.
  static SignalInfoJSCallStack ToSignalInfoJSCallStack(
      const extensions::StackTrace& stack_trace);
  static extensions::StackTrace ToExtensionsStackTrace(
      const SignalInfoJSCallStack& callstack);
  static std::string GetUniqueId(const SignalInfoJSCallStack& callstack);
  // Used for debug logging only.
  static std::string SignalInfoJSCallStackAsString(
      const SignalInfoJSCallStack& callstack);

 protected:
  base::flat_map<std::string, SignalInfoJSCallStack> siginfo_callstack_map_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_JS_CALLSTACKS_H_
