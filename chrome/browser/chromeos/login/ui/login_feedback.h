// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_FEEDBACK_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_FEEDBACK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace chromeos {

class FeedbackExtensionLoader;

// Show the feedback UI to collect a feedback on the login screen. Note that
// it dynamically loads/unloads the feedback extension on the signin profile.
class LoginFeedback {
 public:
  explicit LoginFeedback(Profile* signin_profile);
  ~LoginFeedback();

  // Request to show the feedback UI with |description|. |finished_callback|
  // will be invoked when the feedback UI is closed, either cancel or send the
  // feedback.
  void Request(const std::string& description,
               base::OnceClosure finished_callback);

 private:
  // Makes the feedback UI windows on top of login screen and watches when
  // all feedback windows are closed.
  class FeedbackWindowHandler;

  // Invoked by FeedbackWindowHandler when all feedback windows are closed.
  void OnFeedbackFinished();

  // Ensures feedback UI is created.
  void EnsureFeedbackUI();

  Profile* const profile_;
  std::string description_;
  base::OnceClosure finished_callback_;

  std::unique_ptr<FeedbackWindowHandler> feedback_window_handler_;

  std::unique_ptr<FeedbackExtensionLoader> feedback_extension_loader_;

  base::WeakPtrFactory<LoginFeedback> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginFeedback);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_FEEDBACK_H_
