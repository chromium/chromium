// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Loosely defined states a {@link Pane} should be in. Based on the specified state a pane should
 * change its behavior to use more or less resources and alter its visibility.
 *
 * <p>New entries should be added before {@link LoadHint.COUNT}.
 */
@IntDef({LoadHint.COLD, LoadHint.WARM, LoadHint.HOT, LoadHint.COUNT})
@Retention(RetentionPolicy.SOURCE)
public @interface LoadHint {
    /**
     * The pane and Hub are not visible to users. The pane should use as few resources as possible.
     */
    int COLD = 0;

    /** The pane is not visible to users, but the Hub is. The pane should use fewer resources. */
    int WARM = 1;

    /** The pane is visible to users now and can use any resources needed. */
    int HOT = 2;

    /** Must be last. */
    int COUNT = 3;
}
