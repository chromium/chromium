// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.getBookmarksPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.getHistoryPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.getIncognitoBookmarksPageOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.getIncognitoNtpOverrideEnabled;
import static org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry.getNtpOverrideEnabled;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.download.home.DownloadPage;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.browser_controls.BrowserControlsMarginAdapter;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.management.ManagementPage;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.IncognitoNtpMetrics;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageCreationTracker;
import org.chromium.chrome.browser.ntp.RecentTabsManager;
import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
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
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Creates NativePage objects to show chrome-native:// URLs using the native Android view system.
 */
@NullMarked
public class NativePageFactory {
    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsManager mBrowserControlsManager;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<Toolbar> mToolbarSupplier;
    private final @Nullable HomeSurfaceTracker mHomeSurfaceTracker;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private final StartupMetricsTracker mStartupMetricsTracker;
    private @Nullable NewTabPageCreationTracker mNewTabPageCreationTracker;

    private @Nullable NativePageBuilder mNativePageBuilder;
    private static @Nullable NativePage sTestPage;
    private final BackPressManager mBackPressManager;
    private final MultiInstanceManager mMultiInstanceManager;

    public NativePageFactory(
            Activity activity,
            BottomSheetController sheetController,
            BrowserControlsManager browserControlsManager,
            Supplier<@Nullable Tab> currentTabSupplier,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector,
            Supplier<ShareDelegate> shareDelegateSupplier,
            WindowAndroid windowAndroid,
            Supplier<Toolbar> toolbarSupplier,
            @Nullable HomeSurfaceTracker homeSurfaceTracker,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier,
            StartupMetricsTracker startupMetricsTracker,
            BackPressManager backPressManager,
            MultiInstanceManager multiInstanceManager) {
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
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mModuleRegistrySupplier = moduleRegistrySupplier;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
        mStartupMetricsTracker = startupMetricsTracker;
        mBackPressManager = backPressManager;
        mMultiInstanceManager = multiInstanceManager;
    }

    private NativePageBuilder getBuilder() {
        if (mNativePageBuilder == null) {
            mNativePageBuilder =
                    new NativePageBuilder(
                            mActivity,
                            this::getNewTabPageCreationTracker,
                            mBottomSheetController,
                            mBrowserControlsManager,
                            mCurrentTabSupplier,
                            mSnackbarManagerSupplier,
                            mLifecycleDispatcher,
                            mTabModelSelector,
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            mToolbarSupplier,
                            mHomeSurfaceTracker,
                            mTabContentManagerSupplier,
                            mTabStripHeightSupplier,
                            mModuleRegistrySupplier,
                            mEdgeToEdgeControllerSupplier,
                            mTopInsetCoordinatorSupplier,
                            mStartupMetricsTracker,
                            mBackPressManager,
                            mMultiInstanceManager);
        }
        return mNativePageBuilder;
    }

    private NewTabPageCreationTracker getNewTabPageCreationTracker() {
        if (mNewTabPageCreationTracker == null) {
            mNewTabPageCreationTracker = new NewTabPageCreationTracker(mTabModelSelector);
            mNewTabPageCreationTracker.monitorNtpCreation();
        }
        return mNewTabPageCreationTracker;
    }

    @VisibleForTesting
    static class NativePageBuilder {
        private final Activity mActivity;
        private final BottomSheetController mBottomSheetController;
        private final Supplier<NewTabPageCreationTracker> mNewTabPageCreationTracker;
        private final BrowserControlsManager mBrowserControlsManager;
        private final Supplier<@Nullable Tab> mCurrentTabSupplier;
        private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
        private final ActivityLifecycleDispatcher mLifecycleDispatcher;
        private final TabModelSelector mTabModelSelector;
        private final Supplier<ShareDelegate> mShareDelegateSupplier;
        private final WindowAndroid mWindowAndroid;
        private final Supplier<Toolbar> mToolbarSupplier;
        private final @Nullable HomeSurfaceTracker mHomeSurfaceTracker;
        private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
        private final ObservableSupplier<Integer> mTabStripHeightSupplier;
        private final OneshotSupplier<ModuleRegistry> mModuleRegistrySupplier;
        private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
        private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
        private final StartupMetricsTracker mStartupMetricsTracker;
        private final BackPressManager mBackPressManager;
        private final MultiInstanceManager mMultiInstanceManager;

        public NativePageBuilder(
                Activity activity,
                Supplier<NewTabPageCreationTracker> newTabPageCreationTracker,
                BottomSheetController sheetController,
                BrowserControlsManager browserControlsManager,
                Supplier<@Nullable Tab> currentTabSupplier,
                Supplier<SnackbarManager> snackbarManagerSupplier,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                TabModelSelector tabModelSelector,
                Supplier<ShareDelegate> shareDelegateSupplier,
                WindowAndroid windowAndroid,
                Supplier<Toolbar> toolbarSupplier,
                @Nullable HomeSurfaceTracker homeSurfaceTracker,
                ObservableSupplier<TabContentManager> tabContentManagerSupplier,
                ObservableSupplier<Integer> tabStripHeightSupplier,
                OneshotSupplier<ModuleRegistry> moduleRegistrySupplier,
                ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
                ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier,
                StartupMetricsTracker startupMetricsTracker,
                BackPressManager backPressManager,
                MultiInstanceManager multiInstanceManager) {
            mActivity = activity;
            mNewTabPageCreationTracker = newTabPageCreationTracker;
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
            mTabStripHeightSupplier = tabStripHeightSupplier;
            mModuleRegistrySupplier = moduleRegistrySupplier;
            mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
            mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
            mStartupMetricsTracker = startupMetricsTracker;
            mBackPressManager = backPressManager;
            mMultiInstanceManager = multiInstanceManager;
        }

        protected NativePage buildNewTabPage(Tab tab, String url) {
            NativePageHost nativePageHost =
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier);
            if (tab.isIncognito()) {
                return new IncognitoNewTabPage(
                        mActivity,
                        nativePageHost,
                        tab,
                        mEdgeToEdgeControllerSupplier,
                        createIncognitoNtpMetrics());
            }

            return new NewTabPage(
                    mActivity,
                    mBrowserControlsManager,
                    mCurrentTabSupplier,
                    mSnackbarManagerSupplier.get(),
                    mLifecycleDispatcher,
                    mTabModelSelector,
                    DeviceFormFactor.isWindowOnTablet(mWindowAndroid),
                    mNewTabPageCreationTracker.get(),
                    ColorUtils.inNightMode(mActivity),
                    nativePageHost,
                    tab,
                    url,
                    mBottomSheetController,
                    mShareDelegateSupplier,
                    mWindowAndroid,
                    mToolbarSupplier,
                    mHomeSurfaceTracker,
                    mTabContentManagerSupplier,
                    mTabStripHeightSupplier,
                    mModuleRegistrySupplier,
                    mEdgeToEdgeControllerSupplier,
                    mTopInsetCoordinatorSupplier,
                    mStartupMetricsTracker,
                    mMultiInstanceManager);
        }

        protected NativePage buildBookmarksPage(Tab tab) {
            return new BookmarkPage(
                    mSnackbarManagerSupplier.get(),
                    tab.getProfile(),
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier),
                    mActivity.getComponentName(),
                    mBackPressManager);
        }

        protected NativePage buildDownloadsPage(Tab tab) {
            Profile profile = tab.getProfile();
            return new DownloadPage(
                    mActivity,
                    mSnackbarManagerSupplier.get(),
                    mWindowAndroid.getModalDialogManager(),
                    profile.getOtrProfileId(),
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier),
                    mBackPressManager);
        }

        protected NativePage buildHistoryPage(Tab tab, String url) {
            return new HistoryPage(
                    mActivity,
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier),
                    mSnackbarManagerSupplier.get(),
                    tab.getProfile(),
                    mBottomSheetController,
                    mCurrentTabSupplier,
                    url,
                    mBackPressManager);
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
                                            mActivity, tab, tab.getProfile()));

            NativePageHost host =
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier);
            NativePageNavigationDelegate navigationDelegate =
                    new NativePageNavigationDelegateImpl(
                            mActivity,
                            tab.getProfile(),
                            host,
                            mTabModelSelector,
                            tab,
                            mMultiInstanceManager);

            return new RecentTabsPage(
                    mActivity,
                    recentTabsManager,
                    navigationDelegate,
                    mBrowserControlsManager,
                    mTabStripHeightSupplier,
                    mEdgeToEdgeControllerSupplier);
        }

        protected NativePage buildManagementPage(Tab tab) {
            return new ManagementPage(
                    new TabShim(
                            tab,
                            mBrowserControlsManager,
                            mTabModelSelector,
                            mEdgeToEdgeControllerSupplier),
                    tab.getProfile());
        }

        protected NativePage buildPdfPage(Tab tab, String url, PdfInfo pdfInfo) {
            return NativePageFactory.buildPdfPage(
                    url, tab, pdfInfo, mBrowserControlsManager, mTabModelSelector, mActivity);
        }

        private @Nullable IncognitoNtpMetrics createIncognitoNtpMetrics() {
            if (ChromeFeatureList.sRecordIncognitoNtpTimeToFirstNavigationMetric.isEnabled()) {
                return new IncognitoNtpMetrics();
            }
            return null;
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
    public @Nullable NativePage createNativePage(
            String url, @Nullable NativePage candidatePage, Tab tab, @Nullable PdfInfo pdfInfo) {
        return createNativePageForURL(url, candidatePage, tab, tab.isIncognito(), pdfInfo);
    }

    @VisibleForTesting
    @Nullable NativePage createNativePageForURL(
            @Nullable String url,
            @Nullable NativePage candidatePage,
            Tab tab,
            boolean isIncognito,
            @Nullable PdfInfo pdfInfo) {
        if (url == null) return null;

        GURL gurl = new GURL(url);
        if (isChromePageUrlOverriddenByExtension(gurl, isIncognito)) {
            RecordUserAction.record("ChromeSchemePage.OverrideTriggered");
            return null;
        }

        NativePage page;

        switch (NativePage.nativePageType(gurl, candidatePage, isIncognito, pdfInfo != null)) {
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
                assumeNonNull(pdfInfo);
                page = getBuilder().buildPdfPage(tab, url, pdfInfo);
                break;
            default:
                assert false;
                return null;
        }
        if (page != null) page.updateForUrl(url);
        return page;
    }

    /**
     * Returns whether the given url is for a chrome:// scheme page that is being overridden by an
     * extension.
     *
     * <p>chrome-native:// scheme pages are not affected by this.
     *
     * @param url The url to be checked.
     * @param isIncognito Whether the page is to be displayed in incognito mode.
     */
    private static boolean isChromePageUrlOverriddenByExtension(GURL url, boolean isIncognito) {
        if (!ChromeFeatureList.sChromeNativeUrlOverriding.isEnabled()
                || !UrlConstants.CHROME_SCHEME.equals(url.getScheme())) {
            return false;
        }

        String host = url.getHost();
        if (UrlConstants.NTP_HOST.equals(host)) {
            return isIncognito ? getIncognitoNtpOverrideEnabled() : getNtpOverrideEnabled();
        } else if (UrlConstants.BOOKMARKS_HOST.equals(host)) {
            return isIncognito
                    ? getIncognitoBookmarksPageOverrideEnabled()
                    : getBookmarksPageOverrideEnabled();
        } else if (UrlConstants.HISTORY_HOST.equals(host)) {
            return !isIncognito && getHistoryPageOverrideEnabled();
        }
        return false;
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
    public static @Nullable NativePage createNativePageForCustomTab(
            String url,
            @Nullable NativePage candidatePage,
            Tab tab,
            @Nullable PdfInfo pdfInfo,
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
                            url,
                            tab,
                            assumeNonNull(pdfInfo),
                            browserControlsManager,
                            tabModelSelector,
                            activity);
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
                new TabShim(tab, browserControlsManager, tabModelSelector, null),
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
        private final @Nullable ObservableSupplier<EdgeToEdgeController>
                mEdgeToEdgeControllerSupplier;

        public TabShim(
                Tab tab,
                BrowserControlsStateProvider browserControlsStateProvider,
                TabModelSelector tabModelSelector,
                @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
            mTab = tab;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mTabModelSelector = tabModelSelector;
            mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
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
        public Destroyable createDefaultMarginAdapter(ObservableSupplierImpl<Rect> supplierImpl) {
            return BrowserControlsMarginAdapter.create(mBrowserControlsStateProvider, supplierImpl);
        }

        @Override
        public EdgeToEdgePadAdjuster createEdgeToEdgePadAdjuster(View view) {
            return EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                    view, mEdgeToEdgeControllerSupplier);
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mNewTabPageCreationTracker != null) mNewTabPageCreationTracker.destroy();
    }

    public static void setPdfPageForTesting(PdfPage pdfPage) {
        sTestPage = pdfPage;
    }
}
