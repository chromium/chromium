// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Combination of display information and the event handling for pressing the button. */
interface FullButtonData extends DisplayButtonData {
    /** Returns the {@link Runnable} that should be invoked when the button is pressed. */
    Runnable getOnPressRunnable();
}
