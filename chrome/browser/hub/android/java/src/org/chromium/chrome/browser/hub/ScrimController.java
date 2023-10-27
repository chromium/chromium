// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Interface for controlling scrim visibility. */
public interface ScrimController {
    /** Show a scrim with an animation. */
    void startShowingScrim();

    /** Hide the current scrim with an animation. */
    void startHidingScrim();
}
