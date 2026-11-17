// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_H_

#include "base/auto_reset.h"
#include "base/functional/callback.h"

namespace ttc {

// Plays a short UI attention chime outside the TTC conversation audio stream
// when a Human-on-the-Loop (HOTL) confirmation prompt appears. Must be called
// on the browser UI thread.
void PlayHotlAttentionChime();

// Overrides `PlayHotlAttentionChime()` while the returned `AutoReset` stays
// alive so unit tests can verify chime triggers without playing host OS alerts.
[[nodiscard]] base::AutoReset<base::RepeatingClosure>
SetHotlAttentionChimeCallbackForTesting(base::RepeatingClosure callback);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_H_
