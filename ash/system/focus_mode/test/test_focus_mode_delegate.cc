// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/test/test_focus_mode_delegate.h"

namespace ash {

namespace {

constexpr char kLocale[] = "en-US";

}  // namespace

TestFocusModeDelegate::TestFocusModeDelegate() : locale_(kLocale) {}

TestFocusModeDelegate::~TestFocusModeDelegate() = default;

std::unique_ptr<youtube_music::YouTubeMusicClient>
TestFocusModeDelegate::CreateYouTubeMusicClient(const AccountId&,
                                                const std::string&) {
  // TODO(yongshun): Return the active fake client.
  return nullptr;
}

const std::string& TestFocusModeDelegate::GetLocale() {
  return locale_;
}

bool TestFocusModeDelegate::IsMinorUser() {
  return false;
}

}  // namespace ash
