// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_STATE_H_
#define CC_INPUT_BROWSER_CONTROLS_STATE_H_

namespace cc {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.cc.input
enum class BrowserControlsState {
  // All browser controls should remain fully visible.
  kShown = 1,
  // All browser controls should remain at the minimum allowed shown ratio.
  kHidden = 2,
  // Both states are permitted. In this state, the browser controls will hide
  // when the user scrolls down, and show when the user scrolls up.
  kBoth = 3,
  kMaxValue = kBoth
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_STATE_H_
