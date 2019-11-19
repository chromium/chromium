// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.download.DownloadPage;
import org.chromium.chrome.browser.explore_sites.ExploreSitesPage;
import org.chromium.chrome.browser.feed.FeedNewTabPage;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegateFactory;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.RecentTabsManager;
import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Creates NativePage objects to show chrome-native:// URLs using the native Android view system.
 */
public class NativePageFactory {
    private static NativePageBuilder sNativePageBuilder = new NativePageBuilder();

    @VisibleForTesting
    static class NativePageBuilder {
        protected NativePage buildNewTabPage(ChromeActivity activity, Tab tab,
                TabModelSelector tabModelSelector) {
            ActivityTabProvider activityTabProvider = activity.getActivityTabProvider();
            ActivityLifecycleDispatcher activityLifecycleDispatcher =
                    activity.getLifecycleDispatcher();

            if (tab.isIncognito()) {
                return new IncognitoNewTabPage(activity, new TabShim(tab));
            }

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)) {
                return new FeedNewTabPage(activity, new TabShim(tab), tabModelSelector,
                        activityTabProvider, activityLifecycleDispatcher);
            }

            return new NewTabPage(activity, new TabShim(tab), tabModelSelector, activityTabProvider,
                    activityLifecycleDispatcher);
        }

        protected NativePage buildBookmarksPage(ChromeActivity activity, Tab tab) {
            return new BookmarkPage(activity, new TabShim(tab));
        }

        protected NativePage buildDownloadsPage(ChromeActivity activity, Tab tab) {
            return new DownloadPage(activity, new TabShim(tab));
        }

        protected NativePage buildExploreSitesPage(ChromeActivity activity, Tab tab) {
            return new ExploreSitesPage(activity, new TabShim(tab));
        }

        protected NativePage buildHistoryPage(ChromeActivity activity, Tab tab) {
            return new HistoryPage(activity, new TabShim(tab));
        }

        protected NativePage buildRecentTabsPage(ChromeActivity activity, Tab tab) {
            RecentTabsManager recentTabsManager =
                    new RecentTabsManager(tab, tab.getProfile(), activity);
            return new RecentTabsPage(activity, recentTabsManager, new TabShim(tab));
        }
    }

    @IntDef({NativePageType.NONE, NativePageType.CANDIDATE, NativePageType.NTP,
            NativePageType.BOOKMARKS, NativePageType.RECENT_TABS, NativePageType.DOWNLOADS,
            NativePageType.HISTORY, NativePageType.EXPLORE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NativePageType {
        int NONE = 0;
        int CANDIDATE = 1;
        int NTP = 2;
        int BOOKMARKS = 3;
        int RECENT_TABS = 4;
        int DOWNLOADS = 5;
        int HISTORY = 6;
        int EXPLORE = 7;
    }

    private static @NativePageType int nativePageType(
            String url, NativePage candidatePage, boolean isIncognito) {
        if (url == null) return NativePageType.NONE;

        Uri uri = Uri.parse(url);
        if (!UrlConstants.CHROME_NATIVE_SCHEME.equals(uri.getScheme())) {
            return NativePageType.NONE;
        }

        String host = uri.getHost();
        if (candidatePage != null && candidatePage.getHost().equals(host)) {
            return NativePageType.CANDIDATE;
        }

        if (UrlConstants.NTP_HOST.equals(host)) {
            return NativePageType.NTP;
        } else if (UrlConstants.BOOKMARKS_HOST.equals(host)) {
            return NativePageType.BOOKMARKS;
        } else if (UrlConstants.DOWNLOADS_HOST.equals(host)) {
            return NativePageType.DOWNLOADS;
        } else if (UrlConstants.HISTORY_HOST.equals(host)) {
            return NativePageType.HISTORY;
        } else if (UrlConstants.RECENT_TABS_HOST.equals(host) && !isIncognito) {
            return NativePageType.RECENT_TABS;
        } else if (ExploreSitesPage.isExploreSitesHost(host)) {
            return NativePageType.EXPLORE;
        } else {
            return NativePageType.NONE;
        }
    }

    /**
     * Returns a NativePage for displaying the given URL if the URL is a valid chrome-native URL,
     * or null otherwise. If candidatePage is non-null and corresponds to the URL, it will be
     * returned. Otherwise, a new NativePage will be constructed.
     *
     * @param url The URL to be handled.
     * @param candidatePage A NativePage to be reused if it matches the url, or null.
     * @param tab The Tab that will show the page.
     * @param activity The activity used to create the views for the page.
     * @return A NativePage showing the specified url or null.
     */
    public static NativePage createNativePageForURL(
            String url, NativePage candidatePage, Tab tab, ChromeActivity activity) {
        return createNativePageForURL(url, candidatePage, tab, activity, tab.isIncognito());
    }

    @VisibleForTesting
    static NativePage createNativePageForURL(String url, NativePage candidatePage, Tab tab,
            ChromeActivity activity, boolean isIncognito) {
        NativePage page;

        switch (nativePageType(url, candidatePage, isIncognito)) {
            case NativePageType.NONE:
                return null;
            case NativePageType.CANDIDATE:
                page = candidatePage;
                break;
            case NativePageType.NTP:
                page = sNativePageBuilder.buildNewTabPage(
                        activity, tab, TabModelSelector.from(tab));
                break;
            case NativePageType.BOOKMARKS:
                page = sNativePageBuilder.buildBookmarksPage(activity, tab);
                break;
            case NativePageType.DOWNLOADS:
                page = sNativePageBuilder.buildDownloadsPage(activity, tab);
                break;
            case NativePageType.HISTORY:
                page = sNativePageBuilder.buildHistoryPage(activity, tab);
                break;
            case NativePageType.RECENT_TABS:
                page = sNativePageBuilder.buildRecentTabsPage(activity, tab);
                break;
            case NativePageType.EXPLORE:
                page = sNativePageBuilder.buildExploreSitesPage(activity, tab);
                break;
            default:
                assert false;
                return null;
        }
        if (page != null) page.updateForUrl(url);
        return page;
    }

    /**
     * Returns whether the URL would navigate to a native page.
     *
     * @param url The URL to be checked.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @return Whether the host and the scheme of the passed in URL matches one of the supported
     *         native pages.
     */
    public static boolean isNativePageUrl(String url, boolean isIncognito) {
        return nativePageType(url, null, isIncognito) != NativePageType.NONE;
    }

    @VisibleForTesting
    static void setNativePageBuilderForTesting(NativePageBuilder builder) {
        sNativePageBuilder = builder;
    }

    /** Simple implementation of NativePageHost backed by a {@link Tab} */
    private static class TabShim implements NativePageHost {
        private final Tab mTab;

        public TabShim(Tab mTab) {
            this.mTab = mTab;
        }

        @Override
        public int loadUrl(LoadUrlParams urlParams, boolean incognito) {
            if (incognito && !mTab.isIncognito()) {
                TabModelSelector.from(mTab).openNewTab(urlParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab,
                        /* incognito = */ true);
                return TabLoadStatus.DEFAULT_PAGE_LOAD;
            }

            return mTab.loadUrl(urlParams);
        }

        @Override
        public boolean isIncognito() {
            return mTab.isIncognito();
        }

        @Override
        public int getParentId() {
            return mTab.getParentId();
        }

        @Override
        public Tab getActiveTab() {
            return mTab;
        }

        @Override
        public boolean isVisible() {
            return mTab == TabModelSelector.from(mTab).getCurrentTab();
        }

        @Override
        public HistoryNavigationDelegate createHistoryNavigationDelegate() {
            return HistoryNavigationDelegateFactory.create(mTab);
        }
    }
}
