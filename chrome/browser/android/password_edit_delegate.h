// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_

#include <string>

// An interface used for launching the entry editor and editing a credential
// record.
class PasswordEditDelegate {
 public:
  PasswordEditDelegate() = default;

  PasswordEditDelegate(const PasswordEditDelegate&) = delete;
  PasswordEditDelegate& operator=(const PasswordEditDelegate&) = delete;

  virtual ~PasswordEditDelegate() = default;

  // The method edits a password form held by the delegate. |new_username| and
  // |new_password| are user input from the PasswordEntryEditor.
  virtual void EditSavedPassword(const std::u16string& new_username,
                                 const std::u16string& new_password) = 0;
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_
