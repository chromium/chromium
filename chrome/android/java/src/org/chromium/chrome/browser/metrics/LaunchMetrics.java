// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.webapps.WebApkInfo;
import org.chromium.chrome.browser.webapps.WebApkUkmRecorder;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappInfo;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Used for recording metrics about Chrome launches that need to be recorded before the native
 * library may have been loaded.  Metrics are cached until the library is known to be loaded, then
 * committed to the MetricsService all at once.
 */
@JNINamespace("metrics")
public class LaunchMetrics {
    private static class HomeScreenLaunch {
        public final String mUrl;
        public final boolean mIsShortcut;
        // Corresponds to C++ ShortcutInfo::Source
        public final int mSource;
        public final WebappInfo mWebappInfo;

        public HomeScreenLaunch(String url, boolean isShortcut, int source, WebappInfo webappInfo) {
            mUrl = url;
            mIsShortcut = isShortcut;
            mSource = source;
            mWebappInfo = webappInfo;
        }
    }

    private static final List<HomeScreenLaunch> sHomeScreenLaunches = new ArrayList<>();

    /**
     * Records the launch of a standalone Activity for a URL (i.e. a WebappActivity)
     * added from a specific source.
     * @param webappInfo WebappInfo for launched activity.
     */
    public static void recordHomeScreenLaunchIntoStandaloneActivity(WebappInfo webappInfo) {
        int source = webappInfo.source();

        if (webappInfo.isForWebApk() && source == ShortcutSource.UNKNOWN) {
            // WebApkInfo#source() identifies how the WebAPK was launched (e.g. via deep link).
            // When the WebAPK is launched from the app list (ShortcutSource#UNKNOWN), query
            // WebappDataStorage to determine how the WebAPK was installed (SOURCE_APP_BANNER_WEBAPK
            // vs SOURCE_ADD_TO_HOMESCREEN_PWA). WebAPKs set WebappDataStorage#getSource() at
            // install time.
            source = getSourceForWebApkFromWebappDataStorage(webappInfo);
        }

        sHomeScreenLaunches.add(new HomeScreenLaunch(webappInfo.url(), false, source, webappInfo));
    }

    /**
     * Records the launch of a Tab for a URL (i.e. a Home screen shortcut).
     * @param url URL that kicked off the Tab's creation.
     * @param source integer id of the source from where the URL was added.
     */
    public static void recordHomeScreenLaunchIntoTab(String url, int source) {
        sHomeScreenLaunches.add(new HomeScreenLaunch(url, true, source, null));
    }

    /**
     * Calls out to native code to record URLs that have been launched via the Home screen.
     * This intermediate step is necessary because Activity.onCreate() may be called when
     * the native library has not yet been loaded.
     * @param webContents WebContents for the current Tab.
     */
    public static void commitLaunchMetrics(WebContents webContents) {
        for (HomeScreenLaunch launch : sHomeScreenLaunches) {
            WebappInfo webappInfo = launch.mWebappInfo;
            @WebDisplayMode
            int displayMode =
                    (webappInfo == null) ? WebDisplayMode.UNDEFINED : webappInfo.displayMode();
            LaunchMetricsJni.get().recordLaunch(
                    launch.mIsShortcut, launch.mUrl, launch.mSource, displayMode, webContents);
            if (webappInfo != null && webappInfo.isForWebApk()) {
                WebApkInfo webApkInfo = (WebApkInfo) webappInfo;
                WebApkUkmRecorder.recordWebApkLaunch(webApkInfo.manifestUrl(),
                        webApkInfo.distributor(), webApkInfo.webApkVersionCode(), launch.mSource);
            }
        }
        sHomeScreenLaunches.clear();

        // Record generic cached events.
        CachedMetrics.commitCachedMetrics();
    }

    /**
     * Records metrics about the state of the homepage on launch.
     * @param showHomeButton Whether the home button is shown.
     * @param homepageIsNtp Whether the homepage is set to the NTP.
     * @param homepageUrl The value of the homepage URL.
     */
    public static void recordHomePageLaunchMetrics(
            boolean showHomeButton, boolean homepageIsNtp, String homepageUrl) {
        if (homepageUrl == null) {
            homepageUrl = "";
            assert !showHomeButton : "Homepage should be disabled for a null URL";
        }
        LaunchMetricsJni.get().recordHomePageLaunchMetrics(
                showHomeButton, homepageIsNtp, homepageUrl);
    }

    /**
     * Returns the source from the WebappDataStorage if the source has been stored before. Returns
     * {@link ShortcutSource.WEBAPK_UNKNOWN} otherwise.
     */
    private static int getSourceForWebApkFromWebappDataStorage(WebappInfo webappInfo) {
        WebappDataStorage storage = null;

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            WebappRegistry.warmUpSharedPrefsForId(webappInfo.id());
            storage = WebappRegistry.getInstance().getWebappDataStorage(webappInfo.id());
        }

        if (storage == null) {
            return ShortcutSource.WEBAPK_UNKNOWN;
        }

        int source = storage.getSource();
        return (source == ShortcutSource.UNKNOWN) ? ShortcutSource.WEBAPK_UNKNOWN : source;
    }

    @NativeMethods
    interface Natives {
        void recordLaunch(boolean isShortcut, String url, int source,
                @WebDisplayMode int displayMode, WebContents webContents);
        void recordHomePageLaunchMetrics(
                boolean showHomeButton, boolean homepageIsNtp, String homepageUrl);
    }
}
