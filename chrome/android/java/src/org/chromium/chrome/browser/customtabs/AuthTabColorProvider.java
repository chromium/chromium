// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.browser.auth.AuthTabColorSchemeParams;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.auth.ExperimentalAuthTab;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/** {@link ColorProvider} implementation used for Auth Tab. */
@OptIn(markerClass = ExperimentalAuthTab.class)
public class AuthTabColorProvider implements ColorProvider {
    private static final String TAG = "AuthTabColorProvider";

    private final @ColorInt int mToolbarColor;
    private final @ColorInt int mBottomBarColor;
    private final boolean mHasCustomToolbarColor;
    @Nullable private final Integer mNavigationBarColor;
    @Nullable private final Integer mNavigationBarDividerColor;

    private static @NonNull AuthTabColorSchemeParams getColorSchemeParams(
            Intent intent, int colorScheme) {
        if (colorScheme == COLOR_SCHEME_SYSTEM) {
            assert false
                    : "Color scheme passed to IntentDataProvider should not be "
                            + "COLOR_SCHEME_SYSTEM";
            colorScheme = COLOR_SCHEME_LIGHT;
        }
        try {
            return AuthTabIntent.getColorSchemeParams(intent, colorScheme);
        } catch (Throwable e) {
            // Catch any un-parceling exceptions, like in IntentUtils#safe* methods
            Log.e(TAG, "Failed to parse AuthTabColorSchemeParams");
            return new AuthTabColorSchemeParams.Builder().build(); // Empty params
        }
    }

    public AuthTabColorProvider(
            @NonNull Intent intent,
            @NonNull Context context,
            @CustomTabsIntent.ColorScheme int colorScheme) {
        AuthTabColorSchemeParams params = getColorSchemeParams(intent, colorScheme);
        mHasCustomToolbarColor = params.getToolbarColor() != null;
        mToolbarColor = retrieveToolbarColor(params, context, mHasCustomToolbarColor);
        mBottomBarColor = mToolbarColor;
        mNavigationBarColor =
                params.getNavigationBarColor() == null
                        ? null
                        : ColorUtils.getOpaqueColor(params.getNavigationBarColor());
        mNavigationBarDividerColor = params.getNavigationBarDividerColor();
    }

    private static int retrieveToolbarColor(
            AuthTabColorSchemeParams params, Context context, boolean hasCustomToolbarColor) {
        if (hasCustomToolbarColor) {
            return ColorUtils.getOpaqueColor(params.getToolbarColor());
        }
        return ChromeColors.getDefaultThemeColor(context, /* isIncognito= */ false);
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public boolean hasCustomToolbarColor() {
        return mHasCustomToolbarColor;
    }

    @Override
    public Integer getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    public Integer getNavigationBarDividerColor() {
        return mNavigationBarDividerColor;
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
