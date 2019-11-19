// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INTERACTIVE_TEST_BASE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INTERACTIVE_TEST_BASE_H_

#include <string>

#include "chrome/browser/password_manager/password_manager_test_base.h"

class PasswordManagerInteractiveTestBase
    : public PasswordManagerBrowserTestBase {
 public:
  PasswordManagerInteractiveTestBase();
  ~PasswordManagerInteractiveTestBase() override;

  // Focuses an input element with id |element_id| in the main frame and
  // emulates typing |value| into it. The method waits until the field value
  // switches to |expected_value|.
  void FillElementWithValue(const std::string& element_id,
                            const std::string& value,
                            const std::string& expected_value);

  // Same as above but the field is considered empty before the call.
  void FillElementWithValue(const std::string& element_id,
                            const std::string& value);

  // Replaces the version in PasswordManagerBrowserTestBase which isn't suitable
  // for the interactive UI tests.
  // Wait until the element has |expected_value| value. The value is supposed to
  // be changed by the actual keystrokes.
  void WaitForElementValue(const std::string& element_id,
                           const std::string& expected_value);

  // Navigates to |filename|, fills |username_id| and |password_id| if nonempty
  // and runs |submission_script| to submit. The credential is then saved via
  // the password prompt.
  // Navigates back to |filename| and then verifies that the same elements are
  // filled.
  void VerifyPasswordIsSavedAndFilled(const std::string& filename,
                                      const std::string& username_id,
                                      const std::string& password_id,
                                      const std::string& submission_script);

  // Erases all characters that have been typed into |field_id|.
  void SimulateUserDeletingFieldContent(const std::string& field_id);

 private:
  // WaitForElementValue from the base class don't do the right thing with the
  // real keystrokes.
  using PasswordManagerBrowserTestBase::WaitForElementValue;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_INTERACTIVE_TEST_BASE_H_
