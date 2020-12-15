// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.styles.ChromeColors;

/**
 * CustomTabColorProvider implementation used for normal provides, and some times incognito
 * profiles.
 */
public final class IncognitoCustomTabColorProvider implements CustomTabColorProvider {
    private final int mToolbarColor;
    private final int mBottomBarColor;
    private final int mNavigationBarColor;

    public IncognitoCustomTabColorProvider(Context context) {
        assert context != null;
        mToolbarColor = mBottomBarColor = mNavigationBarColor = ChromeColors.getDefaultThemeColor(
                context.getResources(), /*forceDarkBgColor*/ true);
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public boolean hasCustomToolbarColor() {
        return false;
    }

    @Override
    @Nullable
    public Integer getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    @Nullable
    public Integer getNavigationBarDividerColor() {
        return null;
    }

    @Override
    public int getBottomBarColor() {
        return mBottomBarColor;
    }

    @Override
    public int getInitialBackgroundColor() {
        return Color.TRANSPARENT;
    }
}
