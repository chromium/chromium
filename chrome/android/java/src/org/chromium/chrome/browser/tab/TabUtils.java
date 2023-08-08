// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Collection of utility methods that operates on Tab.
 */
public class TabUtils {
    public static final float THUMBNAIL_ASPECT_RATIO = 0.85f;

    /**
     * Define the callers of NavigationControllerImpl#setUseDesktopUserAgent.
     */
    @IntDef({UseDesktopUserAgentCaller.ON_MENU_OR_KEYBOARD_ACTION,
            UseDesktopUserAgentCaller.LOAD_IF_NEEDED, UseDesktopUserAgentCaller.RELOAD,
            UseDesktopUserAgentCaller.RELOAD_IGNORING_CACHE, UseDesktopUserAgentCaller.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UseDesktopUserAgentCaller {
        int ON_MENU_OR_KEYBOARD_ACTION = 0;
        int LOAD_IF_NEEDED = 100;
        int RELOAD = 200;
        int RELOAD_IGNORING_CACHE = 300;
        int OTHER = 400;
    }

    /**
     * Define the callers of TabImpl#loadIfNeeded.
     */
    @IntDef({LoadIfNeededCaller.SET_TAB, LoadIfNeededCaller.ON_ACTIVITY_SHOWN,
            LoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW, LoadIfNeededCaller.REQUEST_TO_SHOW_TAB,
            LoadIfNeededCaller.REQUEST_TO_SHOW_TAB_THEN_SHOW,
            LoadIfNeededCaller.ON_FINISH_NATIVE_INITIALIZATION,
            LoadIfNeededCaller.MAYBE_SHOW_GLOBAL_SETTING_OPT_IN_MESSAGE, LoadIfNeededCaller.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadIfNeededCaller {
        int SET_TAB = 0;
        int ON_ACTIVITY_SHOWN = 1;
        int ON_ACTIVITY_SHOWN_THEN_SHOW = 2;
        int REQUEST_TO_SHOW_TAB = 3;
        int REQUEST_TO_SHOW_TAB_THEN_SHOW = 4;
        int ON_FINISH_NATIVE_INITIALIZATION = 5;
        int MAYBE_SHOW_GLOBAL_SETTING_OPT_IN_MESSAGE = 6;
        int OTHER = 7;
    }

    // Do not instantiate this class.
    private TabUtils() {}

    /**
     * @return {@link Activity} associated with the given tab.
     */
    public static @Nullable Activity getActivity(Tab tab) {
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
    // status_bar_height is not a public framework resource, so we have to getIdentifier()
    @SuppressWarnings("DiscouragedApi")
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
     * @param tab {@link Tab} instance being checked.
     * @return Whether the tab is detached from any Activity and its {@link WindowAndroid}.
     * Certain functionalities will not work until it is attached to an activity
     * with {@link ReparentingTask#finish}.
     */
    public static boolean isDetached(Tab tab) {
        if (tab.getWebContents() == null) return true;
        // Should get WindowAndroid from WebContents since the one from |getWindowAndroid()|
        // is always non-null even when the tab is in detached state. See the comment in |detach()|.
        WindowAndroid window = tab.getWebContents().getTopLevelNativeWindow();
        if (window == null) return true;
        Activity activity = ContextUtils.activityFromContext(window.getContext().get());
        return !(activity instanceof ChromeActivity);
    }

    /**
     * Call when tab need to switch user agent between desktop and mobile.
     * @param tab The tab to be switched the user agent.
     * @param switchToDesktop Whether switching the user agent to desktop.
     * @param forcedByUser Whether this was triggered by users action.
     * @param caller The caller of this method.
     */
    // TODO(crbug.com/1413060): Remove param forcedByUser from TabUtils#switchUserAgent.
    public static void switchUserAgent(
            Tab tab, boolean switchToDesktop, boolean forcedByUser, int caller) {
        final boolean reloadOnChange = !tab.isNativePage();
        tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                switchToDesktop, reloadOnChange, caller);
        if (forcedByUser) {
            @TabUserAgent
            int tabUserAgent = switchToDesktop ? TabUserAgent.DESKTOP : TabUserAgent.MOBILE;
            if (isDesktopSiteGlobalEnabled(Profile.fromWebContents(tab.getWebContents()))
                    == switchToDesktop) {
                tabUserAgent = TabUserAgent.DEFAULT;
            }
            CriticalPersistedTabData.from(tab).setUserAgent(tabUserAgent);
        }
    }

    /**
     * Get UseDesktopUserAgent setting from webContents.
     * @param webContents The webContents used to retrieve UseDesktopUserAgent setting.
     * @return Whether the webContents is set to use desktop user agent.
     */
    public static boolean isUsingDesktopUserAgent(WebContents webContents) {
        return webContents != null
                && webContents.getNavigationController().getUseDesktopUserAgent();
    }

    /**
     * Get tabUserAgent from the tab, which represents the tab level RDS setting.
     * @param tab The tab used to retrieve tabUserAgent.
     * @return The tab level RDS setting.
     */
    public static @TabUserAgent int getTabUserAgent(Tab tab) {
        @TabUserAgent
        int tabUserAgent = CriticalPersistedTabData.from(tab).getUserAgent();
        WebContents webContents = tab.getWebContents();
        boolean currentRequestDesktopSite = isUsingDesktopUserAgent(webContents);
        // TabUserAgent.UNSET means this is a pre-existing tab from an earlier build. In this case
        // we set the TabUserAgent bit based on last committed entry's user agent. If webContents is
        // null, this method is triggered too early, and we cannot read the last committed entry's
        // user agent yet. We will skip for now and let the following call set the TabUserAgent bit.
        if (webContents != null && tabUserAgent == TabUserAgent.UNSET) {
            if (currentRequestDesktopSite) {
                tabUserAgent = TabUserAgent.DESKTOP;
            } else {
                tabUserAgent = TabUserAgent.DEFAULT;
            }
            CriticalPersistedTabData.from(tab).setUserAgent(tabUserAgent);
        }
        return tabUserAgent;
    }

    /**
     * Read Request Desktop Site ContentSettings.
     * @param profile The profile used to retrieve ContentSettings.
     * @param url The Url used to retrieve site level ContentSettings.
     * @return Whether Request Desktop Site is enabled in ContentSettings.
     */
    public static boolean readRequestDesktopSiteContentSettings(
            Profile profile, @Nullable GURL url) {
        return url != null && TabUtils.isDesktopSiteEnabled(profile, url);
    }

    /**
     * Check if Request Desktop Site ContentSettings is global setting.
     * @param profile The profile used to retrieve ContentSettings.
     * @param url The Url used to retrieve ContentSettings.
     * @return Whether Request Desktop Site ContentSettings is global setting.
     */
    public static boolean isRequestDesktopSiteContentSettingsGlobal(
            Profile profile, @Nullable GURL url) {
        if (url == null) {
            return true;
        }
        return WebsitePreferenceBridge.isContentSettingGlobal(
                profile, ContentSettingsType.REQUEST_DESKTOP_SITE, url, url);
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
     * Return whether hardware keyboard is available, including QWERTY and 12Key keyboards.
     * @param tab The tab used to retrieve context for keyboard configuration.
     * TODO(shuyng): Create ConfigurationChangedObserver to update the current value in C++; to
     * avoid extra JNI request on each navigation.
     */
    @CalledByNative
    public static boolean isHardwareKeyboardAvailable(Tab tab) {
        int keyboard = tab.getContext().getResources().getConfiguration().keyboard;
        return keyboard == Configuration.KEYBOARD_QWERTY
                || keyboard == Configuration.KEYBOARD_12KEY;
    }

    /**
     * Return aspect ratio for grid tab card based on form factor and orientation.
     * @param context - Context of the application.
     * @param browserControlsStateProvider - For getting browser controls height.
     * @return Aspect ratio for the grid tab card.
     */
    public static float getTabThumbnailAspectRatio(
            Context context, BrowserControlsStateProvider browserControlsStateProvider) {
        if ((DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                    || BuildInfo.getInstance().isAutomotive
                    || ChromeFeatureList.sGridTabSwitcherLandscapeAspectRatioPhones.isEnabled())
                && context.getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE) {
            assert browserControlsStateProvider != null;
            int toolbarHeightDp = (browserControlsStateProvider == null)
                    ? 0
                    : Math.round((float) browserControlsStateProvider.getTopControlsHeight()
                            / context.getResources().getDisplayMetrics().density);
            return (context.getResources().getConfiguration().screenWidthDp * 1.f)
                    / (context.getResources().getConfiguration().screenHeightDp * 1.f
                            - toolbarHeightDp);
        }
        return THUMBNAIL_ASPECT_RATIO;
    }

    /**
     * Derive grid card height based on width, expected thumbnail aspect ratio and margins.
     * @param cardWidthPx width of the card
     * @param context to derive view margins
     * @param browserControlsStateProvider - For getting browser controls height.
     * @return computed card height.
     */
    public static int deriveGridCardHeight(int cardWidthPx, Context context,
            BrowserControlsStateProvider browserControlsStateProvider) {
        int tabThumbnailHeight = (int) ((cardWidthPx - getThumbnailWidthDiff(context))
                / getTabThumbnailAspectRatio(context, browserControlsStateProvider));
        int cardHeightPx = tabThumbnailHeight + getThumbnailHeightDiff(context);
        return cardHeightPx;
    }

    /**
     * Derive thumbnail size based on parent card size.
     * @param gridCardSize size of parent card.
     * @param context to derive view margins.
     * @return computed width and height of thumbnail.
     */
    public static Size deriveThumbnailSize(@NonNull Size gridCardSize, @NonNull Context context) {
        int thumbnailWidth = gridCardSize.getWidth() - getThumbnailWidthDiff(context);
        int thumbnailHeight = gridCardSize.getHeight() - getThumbnailHeightDiff(context);
        return new Size(thumbnailWidth, thumbnailHeight);
    }

    private static int getThumbnailHeightDiff(Context context) {
        final int tabGridCardMargin = (int) TabUiThemeProvider.getTabGridCardMargin(context);
        final int thumbnailMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
        int heightMargins = (2 * tabGridCardMargin) + thumbnailMargin;
        final int titleHeight =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_header_height);
        return titleHeight + heightMargins;
    }

    private static int getThumbnailWidthDiff(Context context) {
        final int tabGridCardMargin = (int) TabUiThemeProvider.getTabGridCardMargin(context);
        final int thumbnailMargin =
                (int) context.getResources().getDimension(R.dimen.tab_grid_card_thumbnail_margin);
        return 2 * (tabGridCardMargin + thumbnailMargin);
    }
}
