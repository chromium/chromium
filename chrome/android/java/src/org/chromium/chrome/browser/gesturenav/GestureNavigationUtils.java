// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

/** A set of helper functions related to gesture navigation. */
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
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)) return false;
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
        return ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)
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
        return ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.BACK_FORWARD_TRANSITIONS,
                        "transition_to_native_pages",
                        true);
    }
}
