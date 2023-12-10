// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/** ColorProvider implementation used for normal profiles. */
public final class CustomTabColorProviderImpl implements ColorProvider {
    private static final String TAG = "CustomTabColorPrvdr";

    private final boolean mHasCustomToolbarColor;
    private final int mToolbarColor;
    private final int mBottomBarColor;
    @Nullable private final Integer mNavigationBarColor;
    @Nullable private final Integer mNavigationBarDividerColor;
    private final int mInitialBackgroundColor;

    private static @NonNull CustomTabColorSchemeParams getColorSchemeParams(
            Intent intent, int colorScheme) {
        if (colorScheme == COLOR_SCHEME_SYSTEM) {
            assert false
                    : "Color scheme passed to IntentDataProvider should not be "
                            + "COLOR_SCHEME_SYSTEM";
            colorScheme = COLOR_SCHEME_LIGHT;
        }
        try {
            return CustomTabsIntent.getColorSchemeParams(intent, colorScheme);
        } catch (Throwable e) {
            // Catch any un-parceling exceptions, like in IntentUtils#safe* methods
            Log.e(TAG, "Failed to parse CustomTabColorSchemeParams");
            return new CustomTabColorSchemeParams.Builder().build(); // Empty params
        }
    }

    /**
     * The colorScheme parameter specifies which color scheme the Custom Tab should use.
     * It can currently be either {@link CustomTabsIntent#COLOR_SCHEME_LIGHT} or
     * {@link CustomTabsIntent#COLOR_SCHEME_DARK}.
     * If Custom Tab was launched with {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM}, colorScheme
     * must reflect the current system setting. When the system setting changes, a new
     * CustomTabIntentDataProvider object must be created.
     */
    public CustomTabColorProviderImpl(Intent intent, Context context, int colorScheme) {
        assert intent != null;
        assert context != null;
        CustomTabColorSchemeParams params = getColorSchemeParams(intent, colorScheme);
        mHasCustomToolbarColor = (params.toolbarColor != null);
        mToolbarColor = retrieveToolbarColor(params, context, mHasCustomToolbarColor);
        mBottomBarColor = retrieveBottomBarColor(params, mToolbarColor);
        mNavigationBarColor =
                params.navigationBarColor == null
                        ? null
                        : ColorUtils.getOpaqueColor(params.navigationBarColor);
        mNavigationBarDividerColor = params.navigationBarDividerColor;
        mInitialBackgroundColor = retrieveInitialBackgroundColor(intent);
    }

    /** Returns the color passed from the client app. */
    private static int retrieveToolbarColor(
            CustomTabColorSchemeParams schemeParams,
            Context context,
            boolean hasCustomToolbarColor) {
        int defaultColor =
                ChromeColors.getDefaultThemeColor(context, /* forceDarkBgColor= */ false);
        int color = hasCustomToolbarColor ? schemeParams.toolbarColor : defaultColor;
        return ColorUtils.getOpaqueColor(color);
    }

    private static int retrieveBottomBarColor(
            CustomTabColorSchemeParams schemeParams, int toolbarColor) {
        int color =
                schemeParams.secondaryToolbarColor != null
                        ? schemeParams.secondaryToolbarColor
                        : toolbarColor;
        return ColorUtils.getOpaqueColor(color);
    }

    /**
     * Returns the color to initialize the background of the Custom Tab with.
     * If no valid color is set, Color.TRANSPARENT is returned.
     */
    private static int retrieveInitialBackgroundColor(Intent intent) {
        int defaultColor = Color.TRANSPARENT;
        int color =
                IntentUtils.safeGetIntExtra(
                        intent,
                        CustomTabIntentDataProvider.EXTRA_INITIAL_BACKGROUND_COLOR,
                        defaultColor);
        return color == Color.TRANSPARENT ? color : ColorUtils.getOpaqueColor(color);
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
        return mInitialBackgroundColor;
    }
}
