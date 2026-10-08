// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Collection of util methods for help launching a NewTabPage. */
@NullMarked
public class NewTabPageUtils {
    /** Padding style options for NTP Aurora. */
    @IntDef({PaddingStyle.DEFAULT, PaddingStyle.SMALL, PaddingStyle.MEDIUM, PaddingStyle.LARGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PaddingStyle {
        int DEFAULT = 0;
        int SMALL = 1;
        int MEDIUM = 2;
        int LARGE = 3;
        int NUM_ENTRIES = 4;
    }

    /** Layout type options for NTP Aurora V2. */
    @IntDef({LayoutType.DEFAULT, LayoutType.BESIDE_MVT, LayoutType.INSIDE_MVT, LayoutType.REMOVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutType {
        int DEFAULT = 0;
        int BESIDE_MVT = 1;
        int INSIDE_MVT = 2;
        int REMOVE = 3;
        int NUM_ENTRIES = 4;
    }

    /** Action chips options for NTP Aurora V2. */
    @IntDef({
        ActionChips.DEFAULT,
        ActionChips.INCOGNITO,
        ActionChips.CREATE_IMAGE,
        ActionChips.CANVAS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionChips {
        int DEFAULT = 0;
        int INCOGNITO = 1;
        int CREATE_IMAGE = 2;
        int CANVAS = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Updates the margins for the most visited tiles layout.
     *
     * <p>// TODO(crbug.com/481717794): Re-evaluate all vertical gaps on the NTP. The gap between //
     * the Composeplate (or Search Box) and MVT is currently ~25dp, but should likely be // unified
     * and reduced to 16dp in a future UI polish pass.
     */
    public static void updateTilesLayoutTopMargin(
            View view, boolean shouldShowLogo, boolean isLff) {
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        Resources resources = view.getResources();
        int topMargin =
                resources.getDimensionPixelSize(
                        (shouldShowLogo || isLff)
                                ? R.dimen.ntp_section_top_margin
                                : R.dimen.tile_layout_no_logo_top_margin);

        marginLayoutParams.topMargin = topMargin;
        view.setLayoutParams(marginLayoutParams);
    }

    /** Returns the {@link PaddingStyle} for NTP Aurora. */
    public static @PaddingStyle int getPaddingStyleForAurora() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NTP_AURORA, ChromeFeatureList.NTP_AURORA_PADDING_STYLE);
    }

    /** Returns the space in pixels for NTP sections based on the Aurora padding style. */
    public static int getNtpSectionPaddingPx(Resources resources) {
        if (NewTabPageUtils.getPaddingStyleForAurora() == PaddingStyle.DEFAULT) {
            return resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin);
        } else {
            return resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin_small);
        }
    }

    /** Returns whether the Aurora layout is enabled. */
    public static boolean isNtpAuroraEnabled() {
        return ChromeFeatures.NtpAurora.isEnabled();
    }

    /** Returns whether the Aurora layout V2 is enabled. */
    public static boolean isNtpAuroraV2Enabled() {
        return ChromeFeatures.NtpAurora.isEnabled() && ChromeFeatures.NtpAuroraV2.isEnabled();
    }

    /** Returns whether the Aurora layout with updated button colors is enabled. */
    public static boolean isNtpAuroraButtonColorEnabled() {
        return isNtpAuroraEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.NTP_AURORA,
                        ChromeFeatureList.NTP_AURORA_CHANGE_BUTTON_COLOR);
    }

    /**
     * Returns the type of the action chip to show on the NTP, which is set by the "action_chips"
     * param of NTP Aurora V2. Returns {@link ActionChips#DEFAULT} if NTP Aurora V2 is disabled.
     */
    public static @ActionChips int getMerchandisingChipsType() {
        if (!isNtpAuroraV2Enabled()) return ActionChips.DEFAULT;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NTP_AURORA_V2, ChromeFeatureList.NTP_AURORA_V2_ACTION_CHIPS);
    }

    /**
     * Returns whether clicking the AI Mode button on the NTP should redirect to the AI Mode
     * omnibox, which is the case when a valid, non-default action chip type is enabled.
     */
    public static boolean isAiModeButtonRedirectEnabled() {
        @ActionChips int chipType = getMerchandisingChipsType();
        return chipType > ActionChips.DEFAULT && chipType < ActionChips.NUM_ENTRIES;
    }
}
