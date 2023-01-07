// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_

#include "chrome/browser/extensions/extension_install_prompt.h"

// A helper class to be used with ExtensionInstallPrompt that keeps track of the
// payload. Note that this class does no lifetime management.
class ExtensionInstallPromptTestHelper {
 public:
  ExtensionInstallPromptTestHelper();
  explicit ExtensionInstallPromptTestHelper(base::OnceClosure quit_closure);

  ExtensionInstallPromptTestHelper(const ExtensionInstallPromptTestHelper&) =
      delete;
  ExtensionInstallPromptTestHelper& operator=(
      const ExtensionInstallPromptTestHelper&) = delete;

  ~ExtensionInstallPromptTestHelper();

  // Returns a callback to be used with the ExtensionInstallPrompt.
  ExtensionInstallPrompt::DoneCallback GetCallback();

  // Note: This causes |ADD_FAILURE()| if |payload_| has not been set.
  ExtensionInstallPrompt::DoneCallbackPayload payload() const;

  // Note: This causes |ADD_FAILURE()| if |payload_| has not been set.
  ExtensionInstallPrompt::Result result() const;

  // Note: This causes |ADD_FAILURE()| if |payload_| has not been set.
  std::string justification() const;

  bool has_payload() const { return payload_ != nullptr; }

  // Clears the payload to re-use this test helper.
  // Note: This ADD_FAILURE()s if the payload_ has not been set.
  void ClearPayloadForTesting();

 private:
  void HandlePayload(ExtensionInstallPrompt::DoneCallbackPayload payload);

  std::unique_ptr<ExtensionInstallPrompt::DoneCallbackPayload> payload_;

  // A closure to run once HandlePayload() has been called; used for exiting
  // run loops in tests.
  base::OnceClosure quit_closure_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_TEST_HELPER_H_
