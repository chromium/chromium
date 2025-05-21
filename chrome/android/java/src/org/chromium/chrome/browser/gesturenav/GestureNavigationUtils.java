// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

/** A set of helper functions related to gesture navigation. */
@NullMarked
public class GestureNavigationUtils {

    /**
     * Whether the default nav transition should be allowed for the current tab.
     *
     * @param tab The current tab.
     * @param forward True if navigating forward; false if navigating back.
     * @return True if the transition should be enabled for this tab when navigating..
     */
    public static boolean allowTransition(@Nullable Tab tab, boolean forward) {
        if (tab == null) return false;
        if (!areBackForwardTransitionsEnabled()) return false;
        // If in gesture mode, only U and above support transition.
        if (tab.getWindowAndroid().getWindow() == null) return false;
        if (VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE
                && UiUtils.isGestureNavigationMode(tab.getWindowAndroid().getWindow())) {
            return false;
        }
        if (!allowTransitionFromNativePages() && tab.isNativePage()) return false;
        if (!allowTransitionToNativePages() && navigateToNativePage(tab, forward)) return false;
        return true;
    }

    /**
     * @return Whether the back forward transitions are enabled.
     */
    public static boolean areBackForwardTransitionsEnabled() {
        // Stay in sync with
        // content::BackForwardTransitionAnimationManager::AreBackForwardTransitionsEnabled().
        if (SysUtils.amountOfPhysicalMemoryKB() / 1024
                < ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS,
                        "min-required-physical-ram-mb",
                        0)) {
            return false;
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS);
    }

    /**
     * Whether the tab will navigate back/forward to native pages.
     *
     * @param tab The current tab.
     * @param forward True if navigating forward; false if navigating back.
     * @return True if the tab will navigate to native pages.
     */
    private static boolean navigateToNativePage(@Nullable Tab tab, boolean forward) {
        if (tab == null) return false;
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;
        NavigationHistory navigationHistory =
                webContents.getNavigationController().getNavigationHistory();
        assumeNonNull(navigationHistory);
        NavigationEntry entry =
                navigationHistory.getEntryAtIndex(
                        navigationHistory.getCurrentEntryIndex() + (forward ? 1 : -1));
        return NativePage.isNativePageUrl(entry.getUrl(), tab.isIncognitoBranded(), false);
    }

    /**
     * Whether default nav transitions should be enabled when navigating from native pages.
     *
     * @return True if we should allow default nav transitions when navigating from native pages.
     */
    private static boolean allowTransitionFromNativePages() {
        return GestureNavigationUtils.areBackForwardTransitionsEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS,
                        "transition_from_native_pages",
                        true);
    }

    /**
     * Whether default nav transitions should be enabled when navigating to native pages.
     *
     * @return True if we should allow default nav transitions when navigating to native pages.
     */
    private static boolean allowTransitionToNativePages() {
        return GestureNavigationUtils.areBackForwardTransitionsEnabled()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS,
                        "transition_to_native_pages",
                        true);
    }
}
