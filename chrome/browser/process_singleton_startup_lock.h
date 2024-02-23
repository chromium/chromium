// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_STARTUP_LOCK_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_STARTUP_LOCK_H_

#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chrome/browser/process_singleton.h"

// Provides a ProcessSingleton::NotificationCallback that can queue up
// command-line invocations during startup and execute them when startup
// completes.
//
// The object starts in a locked state. |Unlock()| must be called
// when the process is prepared to handle command-line invocations.
//
// Once unlocked, notifications are forwarded to a wrapped NotificationCallback.
class ProcessSingletonStartupLock {
 public:
  explicit ProcessSingletonStartupLock(
      const ProcessSingleton::NotificationCallback& original_callback);

  ProcessSingletonStartupLock(const ProcessSingletonStartupLock&) = delete;
  ProcessSingletonStartupLock& operator=(const ProcessSingletonStartupLock&) =
      delete;

  ~ProcessSingletonStartupLock();

  // Returns the ProcessSingleton::NotificationCallback.
  // The callback is only valid during the lifetime of the
  // ProcessSingletonStartupLock instance.
  ProcessSingleton::NotificationCallback AsNotificationCallback();

  // Executes previously queued command-line invocations and allows future
  // invocations to be executed immediately.
  void Unlock();

  bool locked() { return locked_; }

 private:
  typedef std::pair<base::CommandLine::StringVector, base::FilePath>
      DelayedStartupMessage;

  bool NotificationCallbackImpl(base::CommandLine command_line,
                                const base::FilePath& current_directory);

  bool locked_;
  std::vector<DelayedStartupMessage> saved_startup_messages_;
  ProcessSingleton::NotificationCallback original_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_STARTUP_LOCK_H_
