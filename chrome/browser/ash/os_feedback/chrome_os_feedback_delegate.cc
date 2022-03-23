// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>

#include "chrome/browser/browser_process.h"

namespace ash {

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate() = default;
ChromeOsFeedbackDelegate::~ChromeOsFeedbackDelegate() = default;

std::string ChromeOsFeedbackDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace ash
