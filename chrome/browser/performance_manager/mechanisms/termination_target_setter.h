// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TERMINATION_TARGET_SETTER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TERMINATION_TARGET_SETTER_H_

namespace performance_manager {

class ProcessNode;

// Mechanism to set a process to terminate on commit failure.
//
// Note: This class is built on all platforms to facilitate development and
// testing, but it's only instantiated in production on Windows, as the
// "commit failure" concept is Windows-specific.
class TerminationTargetSetter {
 public:
  TerminationTargetSetter() = default;
  virtual ~TerminationTargetSetter() = default;
  TerminationTargetSetter(const TerminationTargetSetter& other) = delete;
  TerminationTargetSetter& operator=(const TerminationTargetSetter&) = delete;

  // Sets `process_node` as the process to terminate when a commit failure
  // occurs within the calling process. A call overrides the effect of the
  // previous call. `process_node` can be nullptr to indicate that no process
  // should be terminated on commit failure. The caller must detect when the
  // `process_node`'s underlying process terminates (for any reason) and call
  // this again with a new termination target (or nullptr) - otherwise the
  // implementation may hold onto the terminated process' handle which will
  // undesirably keep the process object alive, see
  // https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/eprocess#eprocess.
  // `ProcessNodeObserver::OnProcessLifetimeChange` may be used to detect this.
  // Virtual to allow overriding in tests.
  virtual void SetTerminationTarget(const ProcessNode* process_node);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_MECHANISMS_TERMINATION_TARGET_SETTER_H_
