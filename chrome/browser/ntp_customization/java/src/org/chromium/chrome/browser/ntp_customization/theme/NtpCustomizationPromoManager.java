// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.time.Duration;

@NullMarked
/** Manager which handles the trigger logic of NTP customization promos, bottom sheet and IPH. */
public class NtpCustomizationPromoManager {
    @IntDef({
        SnackBarState.NOT_SET,
        SnackBarState.PROMO_OPEN,
        SnackBarState.PENDING_ON_RECREATE,
        SnackBarState.SHOWN,
        SnackBarState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SnackBarState {
        int NOT_SET = 0;
        int PROMO_OPEN = 1;
        int PENDING_ON_RECREATE = 2;
        int SHOWN = 3;
        int NUM_ENTRIES = 4;
    }

    private static final long COOL_DOWN_PERIOD_MS = Duration.ofDays(7).toMillis();
    private static final int DURATION_MS = 5000;
    private static @SnackBarState int sState = SnackBarState.NOT_SET;

    // Caches the task Id of the window (activity) in which the theme tip bottom sheet was shown,
    // and the Snackbar should be shown in the same window.
    private static int sTaskIdForRecreate;

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
                && (TimeUtils.currentTimeMillis() - lastApplyTime) < COOL_DOWN_PERIOD_MS) {
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

        if (ChromeFeatureList.sNewTabPageCustomizationV2ShowTipBottomSheet.getValue()) {
            if (!NtpCustomizationUtils.isThemeTipBottomSheetShownFromSharedPreference()) {
                return false;
            }
            long timestamp =
                    NtpCustomizationUtils
                            .getThemeTipBottomSheetShownTimestampFromSharedPreference();
            if (timestamp > 0 && TimeUtils.currentTimeMillis() - timestamp < COOL_DOWN_PERIOD_MS) {
                return false;
            }
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

    /**
     * Shows a homepage customization snackbar if a new theme was recently applied due to the theme
     * tip bottom sheet or the promo card has been shown.
     *
     * @param context The context to load the string resource.
     * @param snackbarManager The {@link SnackbarManager} to display the snackbar.
     * @param taskId The taskId of the current activity.
     */
    public static void maybeShowHomepageCustomizationSnackbarOnRecreate(
            Context context, SnackbarManager snackbarManager, int taskId) {
        if (sState != SnackBarState.PENDING_ON_RECREATE || sTaskIdForRecreate != taskId) {
            return;
        }

        showSnackBar(context, snackbarManager);
    }

    /**
     * Shows a homepage customization snackbar if the appearance bottom sheet is closed without
     * applying a new NTP theme.
     *
     * @param context The application context.
     * @param snackbarManager The {@link SnackbarManager} to display the snackbar.
     */
    public static void maybeShowHomepageCustomizationSnackbarOnDismiss(
            Context context, SnackbarManager snackbarManager) {
        if (sState != SnackBarState.PROMO_OPEN) return;

        showSnackBar(context, snackbarManager);
    }

    private static void showSnackBar(Context context, SnackbarManager snackbarManager) {
        sState = SnackBarState.SHOWN;
        NtpCustomizationUtils.setThemeSnackbarShownToSharedPreference(true);
        String messageText = context.getString(R.string.ntp_customization_theme_snackbar_text);
        Snackbar snackbar =
                Snackbar.make(
                                messageText,
                                null,
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_NTP_THEME_TIP)
                        .setTextAppearance(R.style.TextAppearance_TextMedium_Primary)
                        .setDuration(DURATION_MS);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Updates the showing state of the NTP theme tip snackbar. If the snackbar has been shown
     * before, the state won't be changed.
     *
     * @param newState The new state of the Snakcbar.
     * @param taskId The taskId of the current activity which updates the Snackbar state.
     */
    public static void maybeUpdateShowThemeTipSnackbarState(
            @SnackBarState int newState, int taskId) {
        boolean forceBottomSheetShow =
                ChromeFeatureList.sNewTabPageCustomizationV2ForceShowTipBottomSheet.getValue();
        if ((sState == SnackBarState.SHOWN
                        || NtpCustomizationUtils.isThemeSnackbarShownFromSharedPreference())
                && !forceBottomSheetShow) {
            return;
        }

        if (newState == SnackBarState.PROMO_OPEN) {
            if (sState == SnackBarState.NOT_SET || forceBottomSheetShow) {
                sState = newState;
            }
        } else if (newState == SnackBarState.PENDING_ON_RECREATE) {
            if (sState == SnackBarState.PROMO_OPEN) {
                sState = newState;
                sTaskIdForRecreate = taskId;
            }
        }
    }

    public static void resetForTesting() {
        sState = SnackBarState.NOT_SET;
        sTaskIdForRecreate = 0;
    }

    public static @SnackBarState int getStateForTesting() {
        return sState;
    }

    public static void setStateForTesting(@SnackBarState int state) {
        @SnackBarState int oldValue = sState;
        sState = state;
        ResettersForTesting.register(() -> sState = oldValue);
    }

    public static int getTaskIdForRecreateForTesting() {
        return sTaskIdForRecreate;
    }

    public static void setTaskIdForRecreateForTesting(int taskId) {
        int oldValue = sTaskIdForRecreate;
        sTaskIdForRecreate = taskId;
        ResettersForTesting.register(() -> sTaskIdForRecreate = oldValue);
    }
}
