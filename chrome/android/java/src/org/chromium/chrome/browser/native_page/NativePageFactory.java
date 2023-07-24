// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.download.home.DownloadPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsMarginSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.management.ManagementPage;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.RecentTabsManager;
import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;

/**
 * Creates NativePage objects to show chrome-native:// URLs using the native Android view system.
 */
public class NativePageFactory {
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Supplier<Tab> mCurrentTabSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Toolbar> mToolbarSupplier;
    private final HomeSurfaceTracker mHomeSurfaceTracker;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private NewTabPageUma mNewTabPageUma;

    private NativePageBuilder mNativePageBuilder;

    public NativePageFactory(@NonNull Activity activity,
            @NonNull BottomSheetController sheetController,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Supplier<Tab> currentTabSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull WindowAndroid windowAndroid, @NonNull Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            @Nullable ObservableSupplier<TabContentManager> tabContentManagerSupplier) {
        mActivity = activity;
        mBottomSheetController = sheetController;
        mBrowserControlsManager = browserControlsManager;
        mCurrentTabSupplier = currentTabSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabModelSelector = tabModelSelector;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mToolbarSupplier = toolbarSupplier;
        mHomeSurfaceTracker = homeSurfaceTracker;
        mTabContentManagerSupplier = tabContentManagerSupplier;
    }

    private NativePageBuilder getBuilder() {
        if (mNativePageBuilder == null) {
            mNativePageBuilder = new NativePageBuilder(mActivity, this::getNewTabPageUma,
                    mBottomSheetController, mBrowserControlsManager, mCurrentTabSupplier,
                    mSnackbarManagerSupplier, mLifecycleDispatcher, mTabModelSelector,
                    mShareDelegateSupplier, mWindowAndroid, mToolbarSupplier, mHomeSurfaceTracker,
                    mTabContentManagerSupplier);
        }
        return mNativePageBuilder;
    }

    private NewTabPageUma getNewTabPageUma() {
        if (mNewTabPageUma == null) {
            mNewTabPageUma = new NewTabPageUma(mTabModelSelector);
            mNewTabPageUma.monitorNTPCreation();
        }
        return mNewTabPageUma;
    }

    @VisibleForTesting
    static class NativePageBuilder {
        private final Activity mActivity;
        private final BottomSheetController mBottomSheetController;
        private final Supplier<NewTabPageUma> mUma;
        private final BrowserControlsManager mBrowserControlsManager;
        private final Supplier<Tab> mCurrentTabSupplier;
        private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
        private final ActivityLifecycleDispatcher mLifecycleDispatcher;
        private final TabModelSelector mTabModelSelector;
        private final Supplier<ShareDelegate> mShareDelegateSupplier;
        private final WindowAndroid mWindowAndroid;
        private final Supplier<Toolbar> mToolbarSupplier;
        private final HomeSurfaceTracker mHomeSurfaceTracker;
        private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;

        public NativePageBuilder(Activity activity, Supplier<NewTabPageUma> uma,
                BottomSheetController sheetController,
                BrowserControlsManager browserControlsManager, Supplier<Tab> currentTabSupplier,
                Supplier<SnackbarManager> snackbarManagerSupplier,
                ActivityLifecycleDispatcher lifecycleDispatcher, TabModelSelector tabModelSelector,
                Supplier<ShareDelegate> shareDelegateSupplier, WindowAndroid windowAndroid,
                Supplier<Toolbar> toolbarSupplier, HomeSurfaceTracker homeSurfaceTracker,
                ObservableSupplier<TabContentManager> tabContentManagerSupplier) {
            mActivity = activity;
            mUma = uma;
            mBottomSheetController = sheetController;
            mBrowserControlsManager = browserControlsManager;
            mCurrentTabSupplier = currentTabSupplier;
            mSnackbarManagerSupplier = snackbarManagerSupplier;
            mLifecycleDispatcher = lifecycleDispatcher;
            mTabModelSelector = tabModelSelector;
            mShareDelegateSupplier = shareDelegateSupplier;
            mWindowAndroid = windowAndroid;
            mToolbarSupplier = toolbarSupplier;
            mHomeSurfaceTracker = homeSurfaceTracker;
            mTabContentManagerSupplier = tabContentManagerSupplier;
        }

        protected NativePage buildNewTabPage(Tab tab, String url) {
            NativePageHost nativePageHost =
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector);
            if (tab.isIncognito()) return new IncognitoNewTabPage(mActivity, nativePageHost);

            return new NewTabPage(mActivity, mBrowserControlsManager, mCurrentTabSupplier,
                    mSnackbarManagerSupplier.get(), mLifecycleDispatcher, mTabModelSelector,
                    DeviceFormFactor.isWindowOnTablet(mWindowAndroid), mUma.get(),
                    ColorUtils.inNightMode(mActivity), nativePageHost, tab, url,
                    mBottomSheetController, mShareDelegateSupplier, mWindowAndroid,
                    mToolbarSupplier, new SettingsLauncherImpl(), mHomeSurfaceTracker,
                    mTabContentManagerSupplier);
        }

        protected NativePage buildBookmarksPage(Tab tab) {
            return new BookmarkPage(mActivity.getComponentName(), mSnackbarManagerSupplier.get(),
                    mTabModelSelector.isIncognitoSelected(),
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector));
        }

        protected NativePage buildDownloadsPage(Tab tab) {
            // For preloaded tabs, the tab model might not be initialized yet. Use tab to figure
            // out if it is a regular profile.
            Profile profile = tab.isIncognito() ? mTabModelSelector.getCurrentModel().getProfile()
                                                : Profile.getLastUsedRegularProfile();
            return new DownloadPage(mActivity, mSnackbarManagerSupplier.get(),
                    mWindowAndroid.getModalDialogManager(), profile.getOTRProfileID(),
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector));
        }

        protected NativePage buildHistoryPage(Tab tab, String url) {
            return new HistoryPage(mActivity,
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector),
                    mSnackbarManagerSupplier.get(), mTabModelSelector.isIncognitoSelected(),
                    mCurrentTabSupplier, url);
        }

        protected NativePage buildRecentTabsPage(Tab tab) {
            RecentTabsManager recentTabsManager = new RecentTabsManager(tab, mTabModelSelector,
                    Profile.fromWebContents(tab.getWebContents()), mActivity,
                    ()
                            -> HistoryManagerUtils.showHistoryManager(
                                    mActivity, tab, mTabModelSelector.isIncognitoSelected()));
            return new RecentTabsPage(mActivity, recentTabsManager,
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector),
                    mBrowserControlsManager);
        }

        protected NativePage buildManagementPage(Tab tab) {
            return new ManagementPage(new TabShim(tab, mBrowserControlsManager, mTabModelSelector),
                    Profile.fromWebContents(tab.getWebContents()));
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
                page = getBuilder().buildNewTabPage(tab, url);
                break;
            case NativePageType.BOOKMARKS:
                page = getBuilder().buildBookmarksPage(tab);
                break;
            case NativePageType.DOWNLOADS:
                page = getBuilder().buildDownloadsPage(tab);
                break;
            case NativePageType.HISTORY:
                page = getBuilder().buildHistoryPage(tab, url);
                break;
            case NativePageType.RECENT_TABS:
                page = getBuilder().buildRecentTabsPage(tab);
                break;
            case NativePageType.MANAGEMENT:
                page = getBuilder().buildManagementPage(tab);
                break;
            default:
                assert false;
                return null;
        }
        if (page != null) page.updateForUrl(url);
        return page;
    }

    void setNativePageBuilderForTesting(NativePageBuilder builder) {
        mNativePageBuilder = builder;
    }

    /** Simple implementation of NativePageHost backed by a {@link Tab} */
    private static class TabShim implements NativePageHost {
        private final Tab mTab;
        private final BrowserControlsStateProvider mBrowserControlsStateProvider;
        private final TabModelSelector mTabModelSelector;

        public TabShim(Tab tab, BrowserControlsStateProvider browserControlsStateProvider,
                TabModelSelector tabModelSelector) {
            mTab = tab;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mTabModelSelector = tabModelSelector;
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
