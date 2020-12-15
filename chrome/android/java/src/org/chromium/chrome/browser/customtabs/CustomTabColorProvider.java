// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.Nullable;

/**
 * Provides the set of colors used by custom tabs.
 */
// TODO(sky): make BrowserServicesIntentDataProvider own this and move methods from
// BrowserServicesIntentDataProvider onto this.
public interface CustomTabColorProvider {
    /**
     * @return The color of the bottom bar.
     */
    int getToolbarColor();

    /**
     * @return Whether the intent specifies a custom toolbar color.
     */
    boolean hasCustomToolbarColor();

    /**
     * @return The navigation bar color specified in the intent, or null if not specified.
     */
    @Nullable
    Integer getNavigationBarColor();

    /**
     * @return The navigation bar divider color specified in the intent, or null if not specified.
     */
    @Nullable
    Integer getNavigationBarDividerColor();

    /**
     * @return The color of the bottom bar.
     */
    int getBottomBarColor();

    /**
     * @return Initial RGB background color.
     */
    int getInitialBackgroundColor();
}
