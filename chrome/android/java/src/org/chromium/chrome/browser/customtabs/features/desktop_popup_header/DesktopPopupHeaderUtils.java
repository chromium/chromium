// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import android.content.Context;
import android.os.Build;

import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@NullMarked
public class DesktopPopupHeaderUtils {
    /**
     * Checks whether desktop-like contextual popup UI for Custom Tabs is enabled.
     *
     * @param intentDataProvider contains intent data related to the current browser service.
     * @return true when the intent will result in opening a new popup window, otherwise false.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    public static boolean isDesktopPopupHeaderEnabled(
            final BrowserServicesIntentDataProvider intentDataProvider) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ChromeFeatureList.sAndroidWindowPopupCustomTabUi.isEnabled()
                && intentDataProvider.getUiType() == CustomTabsUiType.POPUP;
    }

    @LayoutRes
    public static int getMainLayoutId() {
        return R.layout.custom_tab_desktop_popup_main_layout;
    }

    @LayoutRes
    public static int getHeaderLayoutId() {
        return R.layout.custom_tab_desktop_popup_header_layout;
    }

    @IdRes
    public static int getContentViewId() {
        return R.id.desktop_popup_content;
    }

    @IdRes
    public static int getHeaderViewStubViewId() {
        return R.id.desktop_popup_header;
    }

    /**
     * Calculates the height of the desktop pop-up header in pixels.
     *
     * <p>This method determines the greater of the internal minimum header requirements (based on
     * resource dimensions) and the system-provided caption bar height.
     *
     * @param context The {@link Context} used to retrieve dimension resources.
     * @param appHeaderHeightPx The height of the window caption bar provided by the OS.
     * @return The final height of the desktop pop-up header in pixels.
     */
    public static int getFinalHeaderHeightPx(Context context, int appHeaderHeightPx) {
        final int minimumHeaderHeightPx =
                Math.max(
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_popup_title_bar_min_height),
                        context.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.custom_tabs_popup_title_bar_text_height));
        return Math.max(minimumHeaderHeightPx, appHeaderHeightPx);
    }
}
