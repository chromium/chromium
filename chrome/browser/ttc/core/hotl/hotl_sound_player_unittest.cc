// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_sound_player.h"

#include <optional>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

TEST(HotlAttentionSoundTest, InvokesOverrideCallbackWhileScopedResetAlive) {
  int chime_count = 0;
  {
    base::AutoReset<base::RepeatingClosure> override_reset =
        SetHotlAttentionChimeCallbackForTesting(
            base::BindLambdaForTesting([&] { ++chime_count; }));

    PlayHotlAttentionChime();
    PlayHotlAttentionChime();
    EXPECT_EQ(chime_count, 2);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // After scope exit, Linux is a no-op and ChromeOS returns safely in tests
  // without AccessibilityManager initialized.
  PlayHotlAttentionChime();
  EXPECT_EQ(chime_count, 2);
#endif
}

TEST(HotlAttentionSoundTest, CallbackCanReplaceOverrideReentrantly) {
  int first_count = 0;
  int second_count = 0;
  std::optional<base::AutoReset<base::RepeatingClosure>> initial_reset;
  std::optional<base::AutoReset<base::RepeatingClosure>> nested_reset;

  initial_reset.emplace(
      SetHotlAttentionChimeCallbackForTesting(base::BindLambdaForTesting([&] {
        ++first_count;
        nested_reset.emplace(SetHotlAttentionChimeCallbackForTesting(
            base::BindLambdaForTesting([&] { ++second_count; })));
      })));

  PlayHotlAttentionChime();
  PlayHotlAttentionChime();
  EXPECT_EQ(first_count, 1);
  EXPECT_EQ(second_count, 1);
}

}  // namespace
}  // namespace ttc
