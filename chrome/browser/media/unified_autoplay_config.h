// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_UNIFIED_AUTOPLAY_CONFIG_H_
#define CHROME_BROWSER_MEDIA_UNIFIED_AUTOPLAY_CONFIG_H_

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class UnifiedAutoplayConfig {
 public:
  UnifiedAutoplayConfig() = delete;
  UnifiedAutoplayConfig(const UnifiedAutoplayConfig&) = delete;
  UnifiedAutoplayConfig& operator=(const UnifiedAutoplayConfig&) = delete;

  // Register profile prefs in the pref registry.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable*);

  // Checks whether autoplay should be blocked by user preference. This will be
  // true if the block autoplay pref is true and if the default sound content
  // setting value is not block.
  static bool ShouldBlockAutoplay(Profile*);

  // Checks whether the block autoplay toggle button should be enabled. If it is
  // false it will still be visible but will be disabled.
  static bool IsBlockAutoplayUserModifiable(Profile*);
};

#endif  // CHROME_BROWSER_MEDIA_UNIFIED_AUTOPLAY_CONFIG_H_
