// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

// A helper class to be used with ExtensionInstallPrompt that keeps track of the
// result. Note that this class does no lifetime management.
class ExtensionInstallPromptTestHelper {
 public:
  ExtensionInstallPromptTestHelper();
  explicit ExtensionInstallPromptTestHelper(base::OnceClosure quit_closure);
  ~ExtensionInstallPromptTestHelper();

  // Returns a callback to be used with the ExtensionInstallPrompt.
  ExtensionInstallPrompt::DoneCallback GetCallback();

  // Note: This ADD_FAILURE()s if result_ has not been set.
  ExtensionInstallPrompt::Result result() const;

  bool has_result() const { return result_ != nullptr; }

  // Clears the result to re-use this test helper.
  // Note: This ADD_FAILURE()s if the result_ has not been set.
  void ClearResultForTesting();

 private:
  void HandleResult(ExtensionInstallPrompt::DoneCallbackPayload payload);

  std::unique_ptr<ExtensionInstallPrompt::Result> result_;

  // A closure to run once HandleResult() has been called; used for exiting
  // run loops in tests.
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptTestHelper);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_
