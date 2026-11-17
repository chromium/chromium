// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_sound_player_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ttc::internal {

void PlayAttentionChime() {
  ::MessageBeep(MB_OK);
}

}  // namespace ttc::internal
