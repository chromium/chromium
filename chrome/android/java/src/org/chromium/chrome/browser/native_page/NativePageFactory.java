// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.content.Context;
import android.graphics.Rect;
import android.net.Uri;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.download.DownloadPage;
import org.chromium.chrome.browser.explore_sites.ExploreSitesPage;
import org.chromium.chrome.browser.fullscreen.BrowserControlsMarginSupplier;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.RecentTabsManager;
import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Creates NativePage objects to show chrome-native:// URLs using the native Android view system.
 */
public class NativePageFactory {
    private final ChromeActivity mActivity;
    private NewTabPageUma mNewTabPageUma;

    private NativePageBuilder mNativePageBuilder;

    public NativePageFactory(ChromeActivity activity) {
        mActivity = activity;
    }

    private NativePageBuilder getBuilder() {
        if (mNativePageBuilder == null) {
            mNativePageBuilder = new NativePageBuilder(mActivity, this::getNewTabPageUma);
        }
        return mNativePageBuilder;
    }

    private NewTabPageUma getNewTabPageUma() {
        if (mNewTabPageUma == null) {
            mNewTabPageUma = new NewTabPageUma(mActivity.getTabModelSelector(),
                    mActivity::getLastUserInteractionTime, mActivity.hadWarmStart(),
                    mActivity::getIntent);
            mNewTabPageUma.monitorNTPCreation();
        }
        return mNewTabPageUma;
    }

    @VisibleForTesting
    static class NativePageBuilder {
        private final ChromeActivity mActivity;
        private final Supplier<NewTabPageUma> mUma;

        public NativePageBuilder(ChromeActivity activity, Supplier<NewTabPageUma> uma) {
            mActivity = activity;
            mUma = uma;
        }

        protected NativePage buildNewTabPage(Tab tab) {
            NativePageHost nativePageHost = new TabShim(tab, mActivity);
            if (tab.isIncognito()) return new IncognitoNewTabPage(mActivity, nativePageHost);

            return new NewTabPage(mActivity, mActivity.getFullscreenManager(),
                    mActivity.getActivityTabProvider(), mActivity.getOverviewModeBehavior(),
                    mActivity.getSnackbarManager(), mActivity.getLifecycleDispatcher(),
                    mActivity.getTabModelSelector(), mActivity.isTablet(), mUma.get(),
                    mActivity.getNightModeStateProvider().isInNightMode(), nativePageHost, tab,
                    mActivity.getBottomSheetController());
        }

        protected NativePage buildBookmarksPage(Tab tab) {
            return new BookmarkPage(mActivity, new TabShim(tab, mActivity));
        }

        protected NativePage buildDownloadsPage(Tab tab) {
            return new DownloadPage(mActivity, new TabShim(tab, mActivity));
        }

        protected NativePage buildExploreSitesPage(Tab tab) {
            return new ExploreSitesPage(mActivity, new TabShim(tab, mActivity), tab);
        }

        protected NativePage buildHistoryPage(Tab tab) {
            return new HistoryPage(mActivity, new TabShim(tab, mActivity));
        }

        protected NativePage buildRecentTabsPage(Tab tab) {
            RecentTabsManager recentTabsManager = new RecentTabsManager(tab,
                    Profile.fromWebContents(tab.getWebContents()), mActivity,
                    () -> HistoryManagerUtils.showHistoryManager(mActivity, tab));
            return new RecentTabsPage(mActivity, recentTabsManager, new TabShim(tab, mActivity));
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
        if (!UrlConstants.CHROME_NATIVE_SCHEME.equals(uri.getScheme())
                && !UrlConstants.CHROME_SCHEME.equals(uri.getScheme())) {
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
     * @return A NativePage showing the specified url or null.
     */
    public NativePage createNativePage(String url, NativePage candidatePage, Tab tab) {
        return createNativePageForURL(url, candidatePage, tab, tab.isIncognito());
    }

    @VisibleForTesting
    NativePage createNativePageForURL(
            String url, NativePage candidatePage, Tab tab, boolean isIncognito) {
        NativePage page;

        switch (nativePageType(url, candidatePage, isIncognito)) {
            case NativePageType.NONE:
                return null;
            case NativePageType.CANDIDATE:
                page = candidatePage;
                break;
            case NativePageType.NTP:
                page = getBuilder().buildNewTabPage(tab);
                break;
            case NativePageType.BOOKMARKS:
                page = getBuilder().buildBookmarksPage(tab);
                break;
            case NativePageType.DOWNLOADS:
                page = getBuilder().buildDownloadsPage(tab);
                break;
            case NativePageType.HISTORY:
                page = getBuilder().buildHistoryPage(tab);
                break;
            case NativePageType.RECENT_TABS:
                page = getBuilder().buildRecentTabsPage(tab);
                break;
            case NativePageType.EXPLORE:
                page = getBuilder().buildExploreSitesPage(tab);
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
    void setNativePageBuilderForTesting(NativePageBuilder builder) {
        mNativePageBuilder = builder;
    }

    /** Simple implementation of NativePageHost backed by a {@link Tab} */
    private static class TabShim implements NativePageHost {
        private final Tab mTab;
        private final ChromeFullscreenManager mFullscreenManager;
        private final TabModelSelector mTabModelSelector;

        public TabShim(Tab tab, ChromeActivity activity) {
            mTab = tab;
            mFullscreenManager = activity.getFullscreenManager();
            mTabModelSelector = activity.getTabModelSelector();
        }

        @Override
        public Context getContext() {
            return mTab.getContext();
        }

        @Override
        public void loadUrl(LoadUrlParams urlParams, boolean incognito) {
            if (incognito && !mTab.isIncognito()) {
                mTabModelSelector.openNewTab(urlParams, TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                        mTab, /* incognito = */ true);
                return;
            }

            mTab.loadUrl(urlParams);
        }

        @Override
        public int getParentId() {
            return mTab.getParentId();
        }

        @Override
        public boolean isVisible() {
            return mTab == mTabModelSelector.getCurrentTab();
        }

        @Override
        public DestroyableObservableSupplier<Rect> createDefaultMarginSupplier() {
            return new BrowserControlsMarginSupplier(mFullscreenManager);
        }
    }
}
