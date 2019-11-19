// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_

#include "base/macros.h"
#include "base/strings/string16.h"

// An interface used for launching the entry editor and editing a credential
// record.
class PasswordEditDelegate {
 public:
  PasswordEditDelegate() = default;
  virtual ~PasswordEditDelegate() = default;

  // The method edits a password form held by the delegate. |new_username| and
  // |new_password| are user input from the PasswordEntryEditor.
  virtual void EditSavedPassword(const base::string16& new_username,
                                 const base::string16& new_password) = 0;

  DISALLOW_COPY_AND_ASSIGN(PasswordEditDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_EDIT_DELEGATE_H_
