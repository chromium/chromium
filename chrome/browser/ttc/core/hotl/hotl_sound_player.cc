// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_sound_player.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/ttc/core/hotl/hotl_sound_player_internal.h"

namespace ttc {

namespace {

base::RepeatingClosure& GetOverrideCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

}  // namespace

void PlayHotlAttentionChime() {
  base::RepeatingClosure& override_callback = GetOverrideCallback();
  if (override_callback) {
    CHECK_IS_TEST();
    // Copy locally in case the callback clears or replaces the override.
    base::RepeatingClosure callback = override_callback;
    callback.Run();
    return;
  }
  internal::PlayAttentionChime();
}

base::AutoReset<base::RepeatingClosure> SetHotlAttentionChimeCallbackForTesting(
    base::RepeatingClosure callback) {
  CHECK_IS_TEST();
  return base::AutoReset<base::RepeatingClosure>(&GetOverrideCallback(),
                                                 std::move(callback));
}

}  // namespace ttc
