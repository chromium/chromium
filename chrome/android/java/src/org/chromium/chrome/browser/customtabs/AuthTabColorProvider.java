// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;

/** {@link ColorProvider} implementation used for Auth Tab. */
public class AuthTabColorProvider implements ColorProvider {
    private final @ColorInt int mToolbarColor;
    private final @ColorInt int mBottomBarColor;

    public AuthTabColorProvider(@NonNull Context context) {
        @ColorInt int color = ChromeColors.getDefaultThemeColor(context, /* isIncognito= */ false);
        mToolbarColor = color;
        mBottomBarColor = color;
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
    public Integer getNavigationBarColor() {
        return null;
    }

    @Override
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
