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

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.download.home.DownloadPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsMarginSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.management.ManagementPage;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.RecentTabsManager;
import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.pdf.PdfInfo;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.HomeSurfaceTracker;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
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
    private final JankTracker mJankTracker;
    private final Supplier<Toolbar> mToolbarSupplier;
    private final HomeSurfaceTracker mHomeSurfaceTracker;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private NewTabPageUma mNewTabPageUma;

    private NativePageBuilder mNativePageBuilder;
    private static NativePage sTestPage;

    public NativePageFactory(
            @NonNull Activity activity,
            @NonNull BottomSheetController sheetController,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Supplier<Tab> currentTabSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier,
            @NonNull WindowAndroid windowAndroid,
            @NonNull JankTracker jankTracker,
            @NonNull Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            @Nullable ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            @NonNull ObservableSupplier<Integer> tabStripHeightSupplier,
            @NonNull OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mActivity = activity;
        mBottomSheetController = sheetController;
        mBrowserControlsManager = browserControlsManager;
        mCurrentTabSupplier = currentTabSupplier;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mLifecycleDispatcher = lifecycleDispatcher;
        mTabModelSelector = tabModelSelector;
        mShareDelegateSupplier = shareDelegateSupplier;
        mWindowAndroid = windowAndroid;
        mJankTracker = jankTracker;
        mToolbarSupplier = toolbarSupplier;
        mHomeSurfaceTracker = homeSurfaceTracker;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mModuleRegistrySupplier = moduleRegistrySupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
    }

    private NativePageBuilder getBuilder() {
        if (mNativePageBuilder == null) {
            mNativePageBuilder =
                    new NativePageBuilder(
                            mActivity,
                            this::getNewTabPageUma,
                            mBottomSheetController,
                            mBrowserControlsManager,
                            mCurrentTabSupplier,
                            mSnackbarManagerSupplier,
                            mLifecycleDispatcher,
                            mTabModelSelector,
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            mJankTracker,
                            mToolbarSupplier,
                            mHomeSurfaceTracker,
                            mTabContentManagerSupplier,
                            mTabStripHeightSupplier,
                            mModuleRegistrySupplier,
                            mEdgeToEdgeControllerSupplier);
        }
        return mNativePageBuilder;
    }

    private NewTabPageUma getNewTabPageUma() {
        if (mNewTabPageUma == null) {
            mNewTabPageUma = new NewTabPageUma(mTabModelSelector);
            mNewTabPageUma.monitorNtpCreation();
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
        private final JankTracker mJankTracker;
        private final Supplier<Toolbar> mToolbarSupplier;
        private final HomeSurfaceTracker mHomeSurfaceTracker;
        private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
        private final ObservableSupplier<Integer> mTabStripHeightSupplier;
        private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
        private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

        public NativePageBuilder(
                Activity activity,
                Supplier<NewTabPageUma> uma,
                BottomSheetController sheetController,
                BrowserControlsManager browserControlsManager,
                Supplier<Tab> currentTabSupplier,
                Supplier<SnackbarManager> snackbarManagerSupplier,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                TabModelSelector tabModelSelector,
                Supplier<ShareDelegate> shareDelegateSupplier,
                WindowAndroid windowAndroid,
                JankTracker jankTracker,
                Supplier<Toolbar> toolbarSupplier,
                HomeSurfaceTracker homeSurfaceTracker,
                ObservableSupplier<TabContentManager> tabContentManagerSupplier,
                ObservableSupplier<Integer> tabStripHeightSupplier,
                OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
                ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
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
            mJankTracker = jankTracker;
            mToolbarSupplier = toolbarSupplier;
            mHomeSurfaceTracker = homeSurfaceTracker;
            mTabContentManagerSupplier = tabContentManagerSupplier;
            mTabStripHeightSupplier = tabStripHeightSupplier;
            mModuleRegistrySupplier = moduleRegistrySupplier;
            mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        }

        protected NativePage buildNewTabPage(Tab tab, String url) {
            NativePageHost nativePageHost =
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector);
            if (tab.isIncognito()) {
                return new IncognitoNewTabPage(
                        mActivity, nativePageHost, tab.getProfile(), mEdgeToEdgeControllerSupplier);
            }

            return new NewTabPage(
                    mActivity,
                    mBrowserControlsManager,
                    mCurrentTabSupplier,
                    mWindowAndroid.getModalDialogManager(),
                    mSnackbarManagerSupplier.get(),
                    mLifecycleDispatcher,
                    mTabModelSelector,
                    DeviceFormFactor.isWindowOnTablet(mWindowAndroid),
                    mUma.get(),
                    ColorUtils.inNightMode(mActivity),
                    nativePageHost,
                    tab,
                    url,
                    mBottomSheetController,
                    mShareDelegateSupplier,
                    mWindowAndroid,
                    mJankTracker,
                    mToolbarSupplier,
                    mHomeSurfaceTracker,
                    mTabContentManagerSupplier,
                    mTabStripHeightSupplier,
                    mModuleRegistrySupplier,
                    mEdgeToEdgeControllerSupplier);
        }

        protected NativePage buildBookmarksPage(Tab tab) {
            return new BookmarkPage(
                    mActivity.getComponentName(),
                    mSnackbarManagerSupplier.get(),
                    tab.getProfile(),
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector));
        }

        protected NativePage buildDownloadsPage(Tab tab) {
            Profile profile = tab.getProfile();
            return new DownloadPage(
                    mActivity,
                    mSnackbarManagerSupplier.get(),
                    mWindowAndroid.getModalDialogManager(),
                    profile.getOTRProfileID(),
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector));
        }

        protected NativePage buildHistoryPage(Tab tab, String url) {
            return new HistoryPage(
                    mActivity,
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector),
                    mSnackbarManagerSupplier.get(),
                    tab.getProfile(),
                    mBottomSheetController,
                    mCurrentTabSupplier,
                    url);
        }

        protected NativePage buildRecentTabsPage(Tab tab) {
            RecentTabsManager recentTabsManager =
                    new RecentTabsManager(
                            tab,
                            mTabModelSelector,
                            tab.getProfile(),
                            mActivity,
                            () ->
                                    HistoryManagerUtils.showHistoryManager(
                                            mActivity,
                                            tab,
                                            mTabModelSelector.isIncognitoSelected()));
            return new RecentTabsPage(
                    mActivity,
                    recentTabsManager,
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector),
                    mBrowserControlsManager,
                    mTabStripHeightSupplier);
        }

        protected NativePage buildManagementPage(Tab tab) {
            return new ManagementPage(
                    new TabShim(tab, mBrowserControlsManager, mTabModelSelector), tab.getProfile());
        }

        protected NativePage buildPdfPage(Tab tab, String url, PdfInfo pdfInfo) {
            return NativePageFactory.buildPdfPage(
                    url, tab, pdfInfo, mBrowserControlsManager, mTabModelSelector, mActivity);
        }
    }

    /**
     * Returns a NativePage for displaying the given URL if the URL is a valid chrome-native URL, or
     * represents a pdf file. Otherwise returns null. If candidatePage is non-null and corresponds
     * to the URL, it will be returned. Otherwise, a new NativePage will be constructed.
     *
     * @param url The URL to be handled.
     * @param candidatePage A NativePage to be reused if it matches the url, or null.
     * @param tab The Tab that will show the page.
     * @param pdfInfo Information of the pdf, or null if there is no associated pdf download.
     * @return A NativePage showing the specified url or null.
     */
    public NativePage createNativePage(
            String url, NativePage candidatePage, Tab tab, PdfInfo pdfInfo) {
        return createNativePageForURL(url, candidatePage, tab, tab.isIncognito(), pdfInfo);
    }

    @VisibleForTesting
    NativePage createNativePageForURL(
            String url, NativePage candidatePage, Tab tab, boolean isIncognito, PdfInfo pdfInfo) {
        NativePage page;

        switch (NativePage.nativePageType(url, candidatePage, isIncognito, pdfInfo != null)) {
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
            case NativePageType.PDF:
                page = getBuilder().buildPdfPage(tab, url, pdfInfo);
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

    /**
     * Returns a NativePage for displaying the given URL if the URL represents a pdf file. Otherwise
     * returns null. If candidatePage is non-null and corresponds to the URL, it will be returned.
     * Otherwise, a new NativePage will be constructed.
     *
     * @param url The URL to be handled.
     * @param candidatePage A NativePage to be reused if it matches the url, or null.
     * @param tab The Tab that will show the page.
     * @param pdfInfo Information of the pdf, or null if not pdf.
     * @param browserControlsManager Manages the browser controls.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param activity The current activity which owns the tab.
     * @return A NativePage showing the specified url or null.
     */
    public static NativePage createNativePageForCustomTab(
            String url,
            NativePage candidatePage,
            Tab tab,
            PdfInfo pdfInfo,
            BrowserControlsManager browserControlsManager,
            TabModelSelector tabModelSelector,
            Activity activity) {
        // Only pdf native page is supported on custom tab.
        if (url == null || pdfInfo == null) {
            return null;
        }
        NativePage page;
        if (candidatePage != null && candidatePage.getUrl().equals(url)) {
            page = candidatePage;
        } else {
            page =
                    buildPdfPage(
                            url, tab, pdfInfo, browserControlsManager, tabModelSelector, activity);
        }
        page.updateForUrl(url);
        return page;
    }

    private static NativePage buildPdfPage(
            String url,
            Tab tab,
            PdfInfo pdfInfo,
            BrowserControlsManager browserControlsManager,
            TabModelSelector tabModelSelector,
            Activity activity) {
        if (sTestPage instanceof PdfPage) {
            return sTestPage;
        }
        return new PdfPage(
                new TabShim(tab, browserControlsManager, tabModelSelector),
                tab.getProfile(),
                activity,
                url,
                pdfInfo,
                activity.getString(R.string.pdf_transient_tab_title),
                tab.getId());
    }

    /** Simple implementation of NativePageHost backed by a {@link Tab} */
    private static class TabShim implements NativePageHost {
        private final Tab mTab;
        private final BrowserControlsStateProvider mBrowserControlsStateProvider;
        private final TabModelSelector mTabModelSelector;

        public TabShim(
                Tab tab,
                BrowserControlsStateProvider browserControlsStateProvider,
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
                mTabModelSelector.openNewTab(
                        urlParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                        mTab,
                        /* incognito= */ true);
                return;
            }

            mTab.loadUrl(urlParams);
        }

        @Override
        public void openNewTab(LoadUrlParams urlParams) {
            mTabModelSelector.openNewTab(
                    urlParams, TabLaunchType.FROM_LINK, mTab, mTab.isIncognito());
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
            return new BrowserControlsMarginSupplier(mBrowserControlsStateProvider);
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mNewTabPageUma != null) mNewTabPageUma.destroy();
    }

    public static void setPdfPageForTesting(PdfPage pdfPage) {
        sTestPage = pdfPage;
    }
}
