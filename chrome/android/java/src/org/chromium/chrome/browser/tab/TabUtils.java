// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.Display;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.url.GURL;

/**
 * Collection of utility methods that operates on Tab.
 */
public class TabUtils {
    private static final String REQUEST_DESKTOP_SCREEN_WIDTH_PARAM = "screen_width_dp";

    // Do not instantiate this class.
    private TabUtils() {}

    /**
     * @return {@link Activity} associated with the given tab.
     */
    @Nullable
    public static Activity getActivity(Tab tab) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents == null || webContents.isDestroyed()) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        return window != null ? window.getActivity().get() : null;
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     * @param context The application context.
     * @return The estimated prerender size in pixels.
     */
    public static Rect estimateContentSize(Context context) {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        // The bounds rectangle includes the bottom bar and the custom tabs bar as well.
        Rect screenBounds = new Rect();
        Point screenSize = new Point();
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        display.getSize(screenSize);
        Resources resources = context.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        try {
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }
        screenBounds.set(0,
                resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height),
                screenSize.x, screenSize.y);
        return screenBounds;
    }

    public static Tab fromWebContents(WebContents webContents) {
        return TabImplJni.get().fromWebContents(webContents);
    }

    /**
     * Call when tab need to switch user agent between desktop and mobile.
     * @param tab The tab to be switched the user agent.
     * @param switchToDesktop Whether switching the user agent to desktop.
     * @param forcedByUser Whether this was triggered by users action.
     */
    public static void switchUserAgent(Tab tab, boolean switchToDesktop, boolean forcedByUser) {
        final boolean reloadOnChange = !tab.isNativePage();
        tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                switchToDesktop, reloadOnChange);
        if (forcedByUser) {
            @TabUserAgent
            int tabUserAgent = switchToDesktop ? TabUserAgent.DESKTOP : TabUserAgent.MOBILE;
            if (ContentFeatureList.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_GLOBAL)
                    && isDesktopSiteGlobalEnabled(Profile.fromWebContents(tab.getWebContents()))
                            == switchToDesktop) {
                tabUserAgent = TabUserAgent.DEFAULT;
            }
            CriticalPersistedTabData.from(tab).setUserAgent(tabUserAgent);
        }
    }

    /**
     * Check if the tab is large enough for displaying desktop sites. This method will only check
     * for tablets, if the device is a phone, will return false regardless of tab size.
     * @param tab The tab to be checked if the size is large enough for desktop site.
     * @return Whether or not the screen size is large enough for desktop sites.
     */
    public static boolean isTabLargeEnoughForDesktopSite(Tab tab) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(tab.getContext())) {
            // The device is a phone, do not check the tab size.
            return false;
        }
        Activity activity = getActivity(tab);
        if (activity == null) {
            // It is possible that we are in custom tabs or tests, and need to access the activity
            // differently.
            activity = ApplicationStatus.getLastTrackedFocusedActivity();
            if (activity == null) return false;
        }
        int windowWidth = activity.getWindow().getDecorView().getWidth();
        int minWidthForDesktopSite = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_FOR_TABLETS,
                REQUEST_DESKTOP_SCREEN_WIDTH_PARAM,
                /* Set a very large size as default to serve as a disabled screen width. */ 4096);

        return minWidthForDesktopSite <= windowWidth;
    }

    /**
     * Check if Request Desktop Site global setting is enabled.
     * @param profile The profile of the tab.
     *        Content settings have separate storage for incognito profiles.
     *        For site-specific exceptions the actual profile is needed.
     * @return Whether the desktop site should be requested.
     */
    public static boolean isDesktopSiteGlobalEnabled(Profile profile) {
        return WebsitePreferenceBridge.isCategoryEnabled(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE);
    }

    /**
     * Check if Request Desktop Site global setting is enabled.
     * @param profile The profile of the tab.
     *        Content settings have separate storage for incognito profiles.
     *        For site-specific exceptions the actual profile is needed.
     * @param url The URL for the current web content.
     * @return Whether the desktop site should be requested.
     */
    public static boolean isDesktopSiteEnabled(Profile profile, GURL url) {
        return WebsitePreferenceBridge.getContentSetting(
                       profile, ContentSettingsType.REQUEST_DESKTOP_SITE, url, url)
                == ContentSettingValues.ALLOW;
    }

    /**
     * Return aspect ratio for grid tab card based on form factor and orientation.
     * @param context - Context of the application.
     * @return Aspect ratio for the grid tab card.
     */
    public static float getTabThumbnailAspectRatio(Context context) {
        if (TabUiFeatureUtilities.isTabletGridTabSwitcherPolishEnabled(context)
                && context.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE) {
            return (context.getResources().getConfiguration().screenWidthDp * 1.f)
                    / (context.getResources().getConfiguration().screenHeightDp * 1.f);
        }
        float value = (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
        return MathUtils.clamp(value, 0.5f, 2.0f);
    }
}
