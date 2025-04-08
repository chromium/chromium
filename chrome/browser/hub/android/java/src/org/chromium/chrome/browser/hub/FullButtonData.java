// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.Nullable;

/** Combination of display information and the event handling for pressing the button. */
public interface FullButtonData extends DisplayButtonData {
    /**
     * Returns the {@link Runnable} that should be invoked when the button is pressed. If this
     * returns null the button will be disabled.
     */
    @Nullable
    Runnable getOnPressRunnable();
}
