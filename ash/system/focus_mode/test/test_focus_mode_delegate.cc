// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/test/test_focus_mode_delegate.h"

namespace ash {

TestFocusModeDelegate::TestFocusModeDelegate() = default;

TestFocusModeDelegate::~TestFocusModeDelegate() = default;

std::unique_ptr<youtube_music::YouTubeMusicClient>
TestFocusModeDelegate::CreateYouTubeMusicClient() {
  // TODO(yongshun): Return the active fake client.
  return nullptr;
}

}  // namespace ash
