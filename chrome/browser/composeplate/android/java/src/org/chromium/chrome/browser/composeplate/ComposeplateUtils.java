// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** Utility class for the composeplate view. */
@NullMarked
public class ComposeplateUtils {

    private static @Nullable Boolean sIsEnabledForTesting;

    /**
     * Returns whether the composeplate can be enabled.
     *
     * @param profile The current profile.
     */
    public static boolean isComposeplateEnabled(Profile profile) {
        if (sIsEnabledForTesting != null) {
            return sIsEnabledForTesting;
        }

        return ComposeplateUtilsJni.get().isAimEntrypointEligible(profile);
    }

    /**
     * Returns whether the composeplate button can be shown on NTPs.
     *
     * @param profile The current profile.
     */
    public static boolean canShowComposeplateButtonOnNtp(Profile profile) {
        return !DeviceInfo.isDesktop() && isComposeplateEnabled(profile);
    }

    /**
     * Returns whether the composeplate is enabled by policy.
     *
     * @param profile The current profile.
     */
    public static boolean isEnabledByPolicy(Profile profile) {
        return ComposeplateUtilsJni.get().isEnabledByPolicy(profile);
    }

    /**
     * Applies the appropriate background drawable for the Search Box.
     *
     * @param context Used to get resources.
     * @param view The search box view instance to update.
     * @param applyWhiteBackground Whether to apply a white background.
     */
    public static void applySearchBoxBackground(
            Context context, View view, boolean applyWhiteBackground) {
        if (applyWhiteBackground) {
            if (NewTabPageUtils.isNtpAuroraEnabled()) {
                // When Aurora is enabled on customized image theme, use white mixed with 2% primary
                // tint.
                view.setBackground(
                        context.getDrawable(
                                R.drawable.fake_search_box_white_with_primary_color_alpha_2));
                return;
            }
            // When Aurora is disabled on customized image theme, use pure white.
            applyWhiteBackground(context, view, /* apply= */ true);
            return;
        }

        // Default theme without customized image theme:
        if (NewTabPageUtils.isNtpAuroraEnabled()) {
            view.setBackground(context.getDrawable(R.drawable.fake_search_box_background));
            return;
        }

        // Default theme with Aurora disabled:
        applyWhiteBackground(context, view, /* apply= */ false);
    }

    /**
     * Applies the appropriate background drawable for Composeplate buttons.
     *
     * @param context Used to get resources.
     * @param view The composeplate button view instance to update.
     * @param applyWhiteBackground Whether to apply a white background.
     */
    public static void applyComposeplateBackground(
            Context context, View view, boolean applyWhiteBackground) {
        if (applyWhiteBackground) {
            if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()) {
                // When Aurora button color is enabled on customized image theme, use tinted
                // background.
                view.setBackground(
                        context.getDrawable(
                                R.drawable.fake_search_box_white_with_primary_color_alpha_2));
                return;
            }
            // When Aurora button color is disabled on customized image theme, use pure white.
            applyWhiteBackground(context, view, /* apply= */ true);
            return;
        }

        // Default theme without customized image theme:
        if (NewTabPageUtils.isNtpAuroraButtonColorEnabled()) {
            view.setBackground(context.getDrawable(R.drawable.composeplate_button_background));
            return;
        }

        // Default theme with Aurora button color disabled:
        applyWhiteBackground(context, view, /* apply= */ false);
    }

    /**
     * Applies a white color to the default background drawable and set it as the new background of
     * the view if apply equals to true; otherwise resets to the default background.
     *
     * @param context Used to get resources.
     * @param view The view instance to update.
     * @param apply Whether to apply or reset to the default background.
     */
    public static void applyWhiteBackground(Context context, View view, boolean apply) {
        Drawable background = context.getDrawable(R.drawable.home_surface_search_box_background);
        if (apply) {
            if (background == null) return;

            // Changes the background of the search_box_container to be white.
            GradientDrawable newBackground = (GradientDrawable) background.mutate();
            newBackground.setColor(Color.WHITE);
            view.setBackground(newBackground);
            return;
        }

        // Resets to the default background drawable.
        view.setBackground(background);
    }

    public static void setIsEnabledForTesting(@Nullable Boolean isEnabledForTesting) {
        @Nullable Boolean oldValue = sIsEnabledForTesting;
        sIsEnabledForTesting = isEnabledForTesting;
        ResettersForTesting.register(() -> sIsEnabledForTesting = oldValue);
    }

    /**
     * Returns an instance of ColorStateList which is used to tint icon buttons based on the flag of
     * whether a white background will be applied.
     *
     * @param context Used to get the ColorStateList.
     * @param shouldApplyWhiteBackgroundOnSearchBox Whether a white background will be applied.
     */
    public static @Nullable ColorStateList getSearchBoxIconColorTint(
            Context context, boolean shouldApplyWhiteBackgroundOnSearchBox) {
        if (shouldApplyWhiteBackgroundOnSearchBox) {
            return context.getColorStateList(R.color.default_icon_color_dark);
        }

        return ThemeUtils.getThemedToolbarIconTint(context, BrandedColorScheme.APP_DEFAULT);
    }

    /**
     * Returns the text appearance resource id based on a flag of whether a white background will be
     * applied.
     */
    public static @StyleRes int getSearchBoxTextStyleResId(
            boolean shouldApplyWhiteBackgroundOnSearchBox) {
        if (shouldApplyWhiteBackgroundOnSearchBox) {
            return R.style.TextAppearance_ComposeplateTextMediumDark;
        }

        return R.style.TextAppearance_ComposeplateTextMedium;
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        boolean isAimEntrypointEligible(@JniType("Profile*") Profile profile);

        boolean isEnabledByPolicy(@JniType("Profile*") Profile profile);
    }
}
