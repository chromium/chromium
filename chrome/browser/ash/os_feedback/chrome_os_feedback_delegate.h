// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
#define CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_

#include <string>

#include "ash/webui/os_feedback_ui/os_feedback_delegate.h"

namespace ash {

class ChromeOsFeedbackDelegate : public OsFeedbackDelegate {
 public:
  ChromeOsFeedbackDelegate();
  ~ChromeOsFeedbackDelegate() override;

  ChromeOsFeedbackDelegate(const ChromeOsFeedbackDelegate&) = delete;
  ChromeOsFeedbackDelegate& operator=(const ChromeOsFeedbackDelegate&) = delete;

  // OsFeedbackDelegate:
  std::string GetApplicationLocale() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
