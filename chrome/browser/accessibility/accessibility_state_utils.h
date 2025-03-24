// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_

namespace accessibility_state_utils {

// On ChromeOS returns true if the screen reader is enabled.
// On other platforms returns true if screen reader mode is enabled, which can
// be true if any functionality that needs screen reader compatibility is
// enabled.
bool IsScreenReaderEnabled();

// Overrides |IsScreenReaderEnabled| for testing.
void OverrideIsScreenReaderEnabledForTesting(bool enabled);

// Returns true if Select-to-Speak is enabled on ChromeOS; returns false on
// other platforms.
bool IsSelectToSpeakEnabled();

}  // namespace accessibility_state_utils

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_
