// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_FEEDBACK_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_FEEDBACK_H_

#include <string>

class Profile;

namespace ash {

// Show the feedback UI to collect a feedback on the login screen.
class LoginFeedback {
 public:
  explicit LoginFeedback(Profile* signin_profile);

  LoginFeedback(const LoginFeedback&) = delete;
  LoginFeedback& operator=(const LoginFeedback&) = delete;

  ~LoginFeedback();

  // Request to show the feedback UI with `description`.
  void Request(const std::string& description);

 private:
  Profile* const profile_;
  std::string description_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_FEEDBACK_H_
