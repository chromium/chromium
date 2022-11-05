// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains base classes for fileManagerPrivate API.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_LOGGED_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_LOGGED_EXTENSION_FUNCTION_H_

#include "base/time/time.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// This class adds a logging feature to ExtensionFunction. Logs may be written
// when the function is completed. API functions of fileManagerPrivate should
// inherit this class.
//
// By default, logging is turned off, hence sub classes should call
// set_log_on_completion(true) to enable it, if they want. However, even if
// the logging is turned off, a warning is emitted when a function call is
// very slow. See the implementation of OnResponded() for details.
//
// Logs are written using drive::EventLogger.
class LoggedExtensionFunction : public ExtensionFunction {
 public:
  LoggedExtensionFunction();

 protected:
  ~LoggedExtensionFunction() override;

  // ExtensionFunction overrides.
  void OnResponded() override;

  void SetWarningThresholds(base::TimeDelta slow_threshold,
                            base::TimeDelta very_slow_threshold);

  // Sets the logging on completion flag. By default, logging is turned off.
  void set_log_on_completion(bool log_on_completion) {
    log_on_completion_ = log_on_completion;
  }

 private:
  base::TimeTicks start_time_;
  bool log_on_completion_;

  base::TimeDelta slow_threshold_;
  base::TimeDelta very_slow_threshold_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_LOGGED_EXTENSION_FUNCTION_H_
