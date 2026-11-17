// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "chrome/browser/ttc/core/hotl/hotl_sound_player_internal.h"

namespace ttc::internal {

void PlayAttentionChime() {
  NSBeep();
}

}  // namespace ttc::internal
