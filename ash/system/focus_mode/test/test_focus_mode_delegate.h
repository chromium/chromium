// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_TEST_TEST_FOCUS_MODE_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_TEST_TEST_FOCUS_MODE_DELEGATE_H_

#include "ash/system/focus_mode/focus_mode_delegate.h"

namespace ash {

class TestFocusModeDelegate : public FocusModeDelegate {
 public:
  TestFocusModeDelegate();
  TestFocusModeDelegate(const TestFocusModeDelegate&) = delete;
  TestFocusModeDelegate& operator=(const TestFocusModeDelegate&) = delete;
  ~TestFocusModeDelegate() override;

  // FocusModeDelegate:
  std::unique_ptr<youtube_music::YouTubeMusicClient> CreateYouTubeMusicClient(
      const AccountId& account_id,
      const std::string& device_id) override;
  const std::string& GetLocale() override;
  bool IsMinorUser() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_TEST_TEST_FOCUS_MODE_DELEGATE_H_
