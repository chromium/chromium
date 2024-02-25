// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_

namespace accessibility_state_utils {

// Returns true if a screen reader is enabled on any platform.
bool IsScreenReaderEnabled();

// Overrides |IsScreenReaderEnabled| for testing.
void OverrideIsScreenReaderEnabledForTesting(bool enabled);

// Returns true if Select-to-Speak is enabled on ChromeOS; returns false on
// other platforms.
bool IsSelectToSpeakEnabled();

}  // namespace accessibility_state_utils

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_STATE_UTILS_H_
