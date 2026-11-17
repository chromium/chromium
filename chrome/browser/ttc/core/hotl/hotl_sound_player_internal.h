// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_INTERNAL_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_INTERNAL_H_

namespace ttc::internal {

// Platform-specific implementation for playing the HOTL attention chime.
void PlayAttentionChime();

}  // namespace ttc::internal

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_SOUND_PLAYER_INTERNAL_H_
