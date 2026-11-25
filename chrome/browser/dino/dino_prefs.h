// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DINO_DINO_PREFS_H_
#define CHROME_BROWSER_DINO_DINO_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace dino {

// Maximum number of times the offline Dino game-over bookmark IPH may be shown
// per profile before it is never displayed again.
inline constexpr int kDinoBookmarkPromptMaxTimesShown = 3;

namespace prefs {

// Counter pref compared against `kDinoBookmarkPromptMaxTimesShown` (3) to
// determine whether the Dino bookmark IPH has already reached its per-profile
// display cap.
inline constexpr char kDinoBookmarkPromptMaxTimesShownCounter[] =
    "dino.bookmark_prompt_max_times_shown_counter";

}  // namespace prefs

// Registers Dino profile preferences.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace dino

#endif  // CHROME_BROWSER_DINO_DINO_PREFS_H_
