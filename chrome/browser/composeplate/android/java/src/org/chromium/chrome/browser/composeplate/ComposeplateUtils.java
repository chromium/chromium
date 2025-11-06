// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Utility class for the composeplate view. */
@NullMarked
public class ComposeplateUtils {

    private static boolean sIsEnabledForTesting;

    /**
     * Returns whether the composeplate can be enabled.
     *
     * @param isTablet Whether the device is a tablet.
     * @param profile The current profile.
     */
    public static boolean isComposeplateEnabled(boolean isTablet, Profile profile) {
        if (sIsEnabledForTesting) return true;
        if (!ComposeplateUtilsJni.get().isAimEntrypointEligible(profile)) return false;

        if (!isTablet) return true;

        return ChromeFeatureList.sAndroidComposeplateLFF.isEnabled()
                && ComposeplateUtilsJni.get().isAimEntrypointLFFEligible(profile);
    }

    /**
     * Applies a white color with shadow to the default background drawable and set it as the new
     * background of the view if apply equals to true; otherwise resets to the default background.
     *
     * @param context Used to get resources.
     * @param view The view instance to update.
     * @param apply Whether to apply or reset to the default background.
     */
    public static void applyWhiteBackgroundAndShadow(Context context, View view, boolean apply) {
        Drawable background = context.getDrawable(R.drawable.home_surface_search_box_background);
        if (apply) {
            if (background == null) return;

            // Changes the background of the search_box_container to be white.
            GradientDrawable newBackground = (GradientDrawable) background.mutate();
            newBackground.setColor(Color.WHITE);
            view.setBackground(newBackground);
            view.setElevation(
                    context.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_elevation));
            view.setClipToOutline(true);
            return;
        }

        // Rests to the default background drawable.
        view.setBackground(background);
        view.setElevation(0f);
        view.setClipToOutline(false);
    }

    public static void setIsEnabledForTesting(boolean isEnabledForTesting) {
        boolean oldValue = sIsEnabledForTesting;
        sIsEnabledForTesting = isEnabledForTesting;
        ResettersForTesting.register(() -> sIsEnabledForTesting = oldValue);
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        boolean isAimEntrypointEligible(@JniType("Profile*") Profile profile);

        boolean isAimEntrypointLFFEligible(@JniType("Profile*") Profile profile);
    }
}
