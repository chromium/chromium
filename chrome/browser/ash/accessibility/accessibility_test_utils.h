// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

using ::extensions::ErrorConsole;

// Instantiate this class to get errors and warnings for an extension.
// This will catch console.error and console.warn messages as well as
// any uncaught JS errors in the extension and cause a non-fatal
// test failure as well as log the failure message.
//
// If this is used in the test SetUp, ensure the lifecycle lasts past
// the scope of the SetUp method, perhaps by using a member var, e.g.
// console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
//        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);
class ExtensionConsoleErrorObserver : public ErrorConsole::Observer {
 public:
  ExtensionConsoleErrorObserver(Profile* profile, const char* extension_id);
  virtual ~ExtensionConsoleErrorObserver();

  // ErrorConsole::Observer:
  void OnErrorAdded(const extensions::ExtensionError* error) override;
  void OnErrorConsoleDestroyed() override;

  // Returns whether errors or warnings were received.
  bool HasErrorsOrWarnings();

  // A helper method to return the string content (in UTF8) of the error or
  // warning at the given |index|. This will cause a test failure if there is no
  // such message.
  std::string GetErrorOrWarningAt(size_t index) const;

  // Get the number of errors and warnings received.
  size_t GetErrorsAndWarningsCount() const;

 private:
  std::vector<std::u16string> errors_;
  raw_ptr<ErrorConsole, ExperimentalAsh> error_console_;
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
