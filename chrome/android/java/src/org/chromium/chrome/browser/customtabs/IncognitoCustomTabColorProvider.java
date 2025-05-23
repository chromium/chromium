// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.Color;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;

/** ColorProvider implementation used for incognito profiles. */
@NullMarked
public final class IncognitoCustomTabColorProvider implements ColorProvider {
    private final int mToolbarColor;
    private final int mBottomBarColor;
    private final int mNavigationBarColor;

    public IncognitoCustomTabColorProvider(Context context) {
        assert context != null;
        mToolbarColor =
                mBottomBarColor =
                        mNavigationBarColor =
                                SurfaceColorUpdateUtils.getDefaultThemeColor(
                                        context, /* isIncognito= */ true);
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
    public @Nullable Integer getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    public @Nullable Integer getNavigationBarDividerColor() {
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
