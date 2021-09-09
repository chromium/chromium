// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.annotation.Px;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * The default strategy for setting the height of the custom tab.
 */
public class CustomTabHeightStrategy {
    public static CustomTabHeightStrategy createStrategy(Activity activity, @Px int initialHeight,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        if (initialHeight <= 0) {
            return new CustomTabHeightStrategy();
        }
        return new PartialCustomTabHeightStrategy(activity, initialHeight, lifecycleDispatcher);
    }

    /**
     * Returns false if we didn't change the Window background color, true otherwise.
     */
    public boolean changeBackgroundColorForResizing() {
        return false;
    }
}
