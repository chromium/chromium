// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.Px;

/**
 * The default strategy for setting the height of the custom tab.
 */
public class CustomTabHeightStrategy {
    public static CustomTabHeightStrategy createStrategy(
            CustomTabActivity activity, @Px int initialHeight) {
        if (initialHeight <= 0) {
            return new CustomTabHeightStrategy();
        }
        return new PartialCustomTabHeightStrategy(activity, initialHeight);
    }
}
