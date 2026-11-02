// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.browser.customtabs.CustomTabColorSchemeParams;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.util.ColorUtils;

/** ColorProvider implementation used for normal profiles. */
@NullMarked
public final class CustomTabColorProviderImpl implements ColorProvider {
    private static final String TAG = "CustomTabColorPrvdr";

    private final boolean mHasCustomToolbarColor;
    private final int mToolbarColor;
    private final int mBottomBarColor;
    private final @Nullable Integer mNavigationBarColor;
    private final @Nullable Integer mNavigationBarDividerColor;
    private final int mInitialBackgroundColor;

    /** Used by CustomTabIntentDataProvider to populate CustomTabIntentDataHolder. */
    static CustomTabColorSchemeParams getColorSchemeParams(Intent intent, int colorScheme) {
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
     * Creates a {@link CustomTabColorProviderImpl} from pre-parsed color scheme params and initial
     * background color.
     *
     * @param params {@link CustomTabColorSchemeParams} already resolved for the color scheme in
     *     use.
     * @param context The {@link Context}.
     * @param initialBackgroundColor The initial background color, already opaque or {@link
     *     Color#TRANSPARENT}.
     */
    public CustomTabColorProviderImpl(
            CustomTabColorSchemeParams params,
            Context context,
            @ColorInt int initialBackgroundColor) {
        mHasCustomToolbarColor = (params.toolbarColor != null);
        mToolbarColor = retrieveToolbarColor(params, context, mHasCustomToolbarColor);
        mBottomBarColor = retrieveBottomBarColor(params, mToolbarColor);
        mNavigationBarColor =
                params.navigationBarColor == null
                        ? null
                        : ColorUtils.getOpaqueColor(params.navigationBarColor);
        mNavigationBarDividerColor = params.navigationBarDividerColor;
        mInitialBackgroundColor = initialBackgroundColor;
    }

    /** Returns the color passed from the client app. */
    private static int retrieveToolbarColor(
            CustomTabColorSchemeParams schemeParams,
            Context context,
            boolean hasCustomToolbarColor) {
        int defaultColor = ChromeColors.getDefaultThemeColor(context, /* isIncognito= */ false);
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
     * Returns the color to initialize the background of the Custom Tab with. If no valid color is
     * set, Color.TRANSPARENT is returned. Used by CustomTabIntentDataProvider to populate
     * CustomTabIntentDataHolder.
     */
    static int retrieveInitialBackgroundColor(Intent intent) {
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
