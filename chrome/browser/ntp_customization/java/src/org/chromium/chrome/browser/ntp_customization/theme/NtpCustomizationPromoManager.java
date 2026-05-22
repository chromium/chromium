// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;

import java.time.Duration;

@NullMarked
/** Manager which handles the trigger logic of NTP customization promos, bottom sheet and IPH. */
public class NtpCustomizationPromoManager {
    /**
     * Returns whether to trigger the NTP theme tip bottom sheet showing.
     *
     * @param windowAndroid The instance of {@link WindowAndroid}
     * @param isTablet Whether the current device is a tablet.
     * @param ntpOpenedCount The number of times that a NTP has been opened.
     */
    public static boolean canTriggerCustomizationBottomSheet(
            WindowAndroid windowAndroid, boolean isTablet, int ntpOpenedCount) {
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled(windowAndroid, isTablet)) {
            return false;
        }

        if (!ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.getValue()
                || (NtpCustomizationConfigManager.getInstance().getBackgroundType()
                        != NtpBackgroundType.DEFAULT)) {
            return false;
        }

        if (ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.getValue()) {
            return true;
        }

        // Triggers the bottom sheet if this bottom sheet hasn't been shown before and this isn't
        // the first time that a NTP is open. The bottom sheet is shown once per lifetime.
        if (ntpOpenedCount <= 1
                || NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference()) {
            return false;
        }

        long lastApplyTime = NtpCustomizationUtils.getLastApplyThemeTimestampFromSharedPreference();
        if (lastApplyTime > 0
                && (TimeUtils.uptimeMillis() - lastApplyTime) < Duration.ofDays(7).toMillis()) {
            return false;
        }

        return true;
    }

    /**
     * Returns whether the theme customization IPH should be shown.
     *
     * @param tab The current {@link Tab}.
     * @param windowAndroid The instance of {@link WindowAndroid}
     * @param isTablet Whether the current device is a tablet.
     */
    public static boolean canShowCustomizationIph(
            Tab tab, WindowAndroid windowAndroid, boolean isTablet) {
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled(windowAndroid, isTablet)) {
            return false;
        }

        if (tab.isIncognitoBranded()
                || !UrlUtilities.isNtpUrl(tab.getUrl())
                || NtpCustomizationUtils.getNtpBackgroundType() != NtpBackgroundType.DEFAULT) {
            return false;
        }

        return true;
    }

    /**
     * Returns whether to trigger the customized NTP theme promo.
     *
     * @param windowAndroid The instance of {@link WindowAndroid}
     * @param isTablet Whether the current device is a tablet.
     */
    public static boolean canTriggerCustomizationPromo(
            WindowAndroid windowAndroid, boolean isTablet) {
        return NtpCustomizationUtils.isNtpThemeCustomizationEnabled(windowAndroid, isTablet);
    }
}
