// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_

class PasswordAccessLossWarningBridge {
 public:
  virtual ~PasswordAccessLossWarningBridge() = default;

  virtual void MaybeShowAccessLossNoticeSheet() = 0;
  virtual bool ShouldShowAccessLossNoticeSheet() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
