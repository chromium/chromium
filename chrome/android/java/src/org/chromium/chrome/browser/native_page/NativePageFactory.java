// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsMarginSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.download.DownloadPage;
import org.chromium.chrome.browser.explore_sites.ExploreSitesPage;
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
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.util.ColorUtils;

/**
 * Creates NativePage objects to show chrome-native:// URLs using the native Android view system.
 */
public class NativePageFactory {
    private final ChromeActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private NewTabPageUma mNewTabPageUma;

    private NativePageBuilder mNativePageBuilder;

    public NativePageFactory(ChromeActivity activity, BottomSheetController sheetController) {
        mActivity = activity;
        mBottomSheetController = sheetController;
    }

    private NativePageBuilder getBuilder() {
        if (mNativePageBuilder == null) {
            mNativePageBuilder = new NativePageBuilder(
                    mActivity, this::getNewTabPageUma, mBottomSheetController);
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
        private final BottomSheetController mBottomSheetController;
        private final Supplier<NewTabPageUma> mUma;

        public NativePageBuilder(ChromeActivity activity, Supplier<NewTabPageUma> uma,
                BottomSheetController sheetController) {
            mActivity = activity;
            mUma = uma;
            mBottomSheetController = sheetController;
        }

        protected NativePage buildNewTabPage(Tab tab) {
            NativePageHost nativePageHost = new TabShim(tab, mActivity);
            if (tab.isIncognito()) return new IncognitoNewTabPage(mActivity, nativePageHost);

            return new NewTabPage(mActivity, mActivity.getBrowserControlsManager(),
                    mActivity.getActivityTabProvider(), mActivity.getSnackbarManager(),
                    mActivity.getLifecycleDispatcher(), mActivity.getTabModelSelector(),
                    mActivity.isTablet(), mUma.get(), ColorUtils.inNightMode(mActivity),
                    nativePageHost, tab, mBottomSheetController);
        }

        protected NativePage buildBookmarksPage(Tab tab) {
            return new BookmarkPage(mActivity.getComponentName(), mActivity.getSnackbarManager(),
                    new TabShim(tab, mActivity));
        }

        protected NativePage buildDownloadsPage(Tab tab) {
            return new DownloadPage(mActivity, new TabShim(tab, mActivity));
        }

        protected NativePage buildExploreSitesPage(Tab tab) {
            return new ExploreSitesPage(
                    mActivity, new TabShim(tab, mActivity), tab, mActivity.getTabModelSelector());
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

        switch (NativePage.nativePageType(url, candidatePage, isIncognito)) {
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
     * TODO(crbug.com/1127732): Use NativePage.isNativePageUrl directly.
     * @param url The URL to be checked.
     * @param isIncognito Whether the page will be displayed in incognito mode.
     * @return Whether the host and the scheme of the passed in URL matches one of the supported
     *         native pages.
     */
    public static boolean isNativePageUrl(String url, boolean isIncognito) {
        return NativePage.isNativePageUrl(url, isIncognito);
    }

    @VisibleForTesting
    void setNativePageBuilderForTesting(NativePageBuilder builder) {
        mNativePageBuilder = builder;
    }

    /** Simple implementation of NativePageHost backed by a {@link Tab} */
    private static class TabShim implements NativePageHost {
        private final Tab mTab;
        private final BrowserControlsStateProvider mBrowserControlsStateProvider;
        private final TabModelSelector mTabModelSelector;

        public TabShim(Tab tab, ChromeActivity activity) {
            mTab = tab;
            mBrowserControlsStateProvider = activity.getBrowserControlsManager();
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
            return CriticalPersistedTabData.from(mTab).getParentId();
        }

        @Override
        public boolean isVisible() {
            return mTab == mTabModelSelector.getCurrentTab();
        }

        @Override
        public DestroyableObservableSupplier<Rect> createDefaultMarginSupplier() {
            return new BrowserControlsMarginSupplier(mBrowserControlsStateProvider);
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mNewTabPageUma != null) mNewTabPageUma.destroy();
    }
}
