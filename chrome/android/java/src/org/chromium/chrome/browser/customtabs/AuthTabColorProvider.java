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
import androidx.browser.auth.AuthTabColorSchemeParams;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/** {@link ColorProvider} implementation used for Auth Tab. */
@NullMarked
public class AuthTabColorProvider implements ColorProvider {
    private static final String TAG = "AuthTabColorProvider";

    private final @ColorInt int mToolbarColor;
    private final @ColorInt int mBottomBarColor;
    private final boolean mHasCustomToolbarColor;
    private final @Nullable Integer mNavigationBarColor;
    private final @Nullable Integer mNavigationBarDividerColor;

    private static AuthTabColorSchemeParams getColorSchemeParams(Intent intent, int colorScheme) {
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
            Intent intent, Context context, @CustomTabsIntent.ColorScheme int colorScheme) {
        AuthTabColorSchemeParams params = getColorSchemeParams(intent, colorScheme);
        mHasCustomToolbarColor = params.getToolbarColor() != null;
        mToolbarColor = retrieveToolbarColor(params, context);
        mBottomBarColor = mToolbarColor;
        mNavigationBarColor =
                params.getNavigationBarColor() == null
                        ? null
                        : ColorUtils.getOpaqueColor(params.getNavigationBarColor());
        mNavigationBarDividerColor = params.getNavigationBarDividerColor();
    }

    private static int retrieveToolbarColor(AuthTabColorSchemeParams params, Context context) {
        if (params.getToolbarColor() != null) {
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
    public @Nullable Integer getNavigationBarColor() {
        return mNavigationBarColor;
    }

    @Override
    public @Nullable Integer getNavigationBarDividerColor() {
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
