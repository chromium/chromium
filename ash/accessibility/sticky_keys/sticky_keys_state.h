// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_STATE_H_
#define ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_STATE_H_

namespace ash {

// State of an individual modifier key.
enum StickyKeyState {
  // The sticky key is disabled. Incoming non-modifier key events are not
  // affected.
  STICKY_KEY_STATE_DISABLED,
  // The sticky key is enabled. Incoming non-modifier key down events are
  // modified. After that, sticky key state becomes DISABLED.
  STICKY_KEY_STATE_ENABLED,
  // The sticky key is locked. All incoming non modifier key down events are
  // modified.
  STICKY_KEY_STATE_LOCKED,
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_STATE_H_
