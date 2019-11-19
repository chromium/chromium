// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.support.v7.app.ActionBar;
import android.text.TextUtils;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.base.ObservableSupplierImpl;
import org.chromium.base.metrics.CachedMetrics.ActionEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.metrics.OmniboxStartupMetrics;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.toolbar.bottom.BottomTabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.toolbar.top.ActionModeController;
import org.chromium.chrome.browser.toolbar.top.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.Toolbar;
import org.chromium.chrome.browser.toolbar.top.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_switcher_action_menu.TabSwitcherActionMenuCoordinator;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.MenuButtonDelegate;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighter;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ScrimView.ScrimObserver;
import org.chromium.chrome.browser.widget.ScrimView.ScrimParams;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/**
 * Contains logic for managing the toolbar visual component.  This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
public class ToolbarManager implements ScrimObserver, ToolbarTabController, UrlFocusChangeListener,
                                       ThemeColorObserver, MenuButtonDelegate {
    /**
     * Handle UI updates of menu icons. Only applicable for phones.
     */
    public interface MenuDelegatePhone {
        /**
         * Called when current tab's loading status changes.
         *
         * @param isLoading Whether the current tab is loading.
         */
        void updateReloadButtonState(boolean isLoading);
    }

    private static final ActionEvent ACCELERATOR_BUTTON_TAP_ACTION =
            new ActionEvent("MobileToolbarOmniboxAcceleratorTap");

    /**
     * The number of ms to wait before reporting to UMA omnibox interaction metrics.
     */
    private static final int RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS = 30000;

    /**
     * The minimum load progress that can be shown when a page is loading.  This is not 0 so that
     * it's obvious to the user that something is attempting to load.
     */
    private static final float MINIMUM_LOAD_PROGRESS = 0.05f;

    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TabCountProvider mTabCountProvider;
    private final ThemeColorProvider mTabThemeColorProvider;
    private final AppThemeColorProvider mAppThemeColorProvider;
    private final TopToolbarCoordinator mToolbar;
    private final ToolbarControlContainer mControlContainer;

    private BottomControlsCoordinator mBottomControlsCoordinator;
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelObserver mTabModelObserver;
    private MenuDelegatePhone mMenuDelegatePhone;
    private final LocationBarModel mLocationBarModel;
    private Profile mCurrentProfile;
    private final ObservableSupplierImpl<BookmarkBridge> mBookmarkBridgeSupplier;
    private BookmarkBridge mBookmarkBridge;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private LocationBar mLocationBar;
    private FindToolbarManager mFindToolbarManager;
    private @Nullable AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private OverviewModeBehavior mOverviewModeBehavior;
    private LayoutManager mLayoutManager;
    private IdentityDiscController mIdentityDiscController;
    private final ShareDelegate mShareDelegate;

    private TabObserver mTabObserver;
    private BookmarkBridge.BookmarkModelObserver mBookmarksObserver;
    private FindToolbarObserver mFindToolbarObserver;
    private OverviewModeObserver mOverviewModeObserver;
    private SceneChangeObserver mSceneChangeObserver;
    private final ActionBarDelegate mActionBarDelegate;
    private ActionModeController mActionModeController;
    private final ToolbarActionModeCallback mToolbarActionModeCallback;
    private LoadProgressSimulator mLoadProgressSimulator;
    private final Callback<Boolean> mUrlFocusChangedCallback;
    private final Handler mHandler = new Handler();
    private final ChromeActivity mActivity;
    private UrlFocusChangeListener mLocationBarFocusObserver;
    private OrientationEventListener mOrientationEventListener;

    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private int mFullscreenFocusToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenMenuToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenHighlightToken = TokenHolder.INVALID_TOKEN;

    private int mPreselectedTabId = Tab.INVALID_TAB_ID;

    private boolean mNativeLibraryReady;
    private boolean mTabRestoreCompleted;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private TextBubble mTextBubble;

    private boolean mInitializedWithNative;
    private Runnable mOnInitializedRunnable;

    private boolean mShouldUpdateToolbarPrimaryColor = true;
    private int mCurrentThemeColor;

    private OmniboxStartupMetrics mOmniboxStartupMetrics;

    private boolean mIsBottomToolbarVisible;

    private AppMenuHandler mAppMenuHandler;

    /**
     * Creates a ToolbarManager object.
     *
     * @param controlContainer The container of the toolbar.
     * @param invalidator Handler for synchronizing invalidations across UI elements.
     * @param urlFocusChangedCallback The callback to be notified when the URL focus changes.
     */
    public ToolbarManager(ChromeActivity activity, ToolbarControlContainer controlContainer,
            Invalidator invalidator, Callback<Boolean> urlFocusChangedCallback,
            ThemeColorProvider themeColorProvider, ShareDelegate shareDelegate) {
        mActivity = activity;
        mActionBarDelegate = new ViewShiftingActionBarDelegate(activity, controlContainer);
        mShareDelegate = shareDelegate;

        mLocationBarModel = new LocationBarModel(activity);
        mControlContainer = controlContainer;
        assert mControlContainer != null;
        mUrlFocusChangedCallback = urlFocusChangedCallback;

        mToolbarActionModeCallback = new ToolbarActionModeCallback();
        mBookmarkBridgeSupplier = new ObservableSupplierImpl<>();

        mOrientationEventListener = new OrientationEventListener(activity) {
            @Override
            public void onOrientationChanged(int orientation) {
                onOrientationChange();
            }
        };
        mOrientationEventListener.enable();

        mLocationBarFocusObserver = new UrlFocusChangeListener() {
            /** The params used to control how the scrim behaves when shown for the omnibox. */
            private ScrimParams mScrimParams;

            /** The light color to use for the scrim on the NTP. */
            private int mLightScrimColor;

            @Override
            public void onUrlFocusChange(boolean hasFocus) {
                if (mScrimParams == null) {
                    Resources res = mActivity.getResources();
                    int topMargin = res.getDimensionPixelSize(R.dimen.tab_strip_height);
                    mLightScrimColor = ApiCompatibilityUtils.getColor(
                            res, R.color.omnibox_focused_fading_background_color_light);
                    View scrimTarget = mActivity.getCompositorViewHolder();
                    mScrimParams = new ScrimView.ScrimParams(
                            scrimTarget, true, false, topMargin, ToolbarManager.this);
                }

                boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
                mScrimParams.backgroundColor = !isTablet && !mLocationBarModel.isIncognito()
                                && !mActivity.getNightModeStateProvider().isInNightMode()
                        ? mLightScrimColor
                        : null;

                if (hasFocus && !showScrimAfterAnimationCompletes()) {
                    mActivity.getScrim().showScrim(mScrimParams);
                } else if (!hasFocus) {
                    mActivity.getScrim().hideScrim(true);
                }
            }

            @Override
            public void onUrlAnimationFinished(boolean hasFocus) {
                if (hasFocus && showScrimAfterAnimationCompletes()) {
                    mActivity.getScrim().showScrim(mScrimParams);
                }
            }

            /**
             * @return Whether the scrim should wait to be shown until after the omnibox is done
             *         animating.
             */
            private boolean showScrimAfterAnimationCompletes() {
                if (mLocationBarModel.getNewTabPageForCurrentTab() == null) return false;
                return mLocationBarModel.getNewTabPageForCurrentTab().isLocationBarShownInNTP();
            }
        };

        mIncognitoStateProvider = new IncognitoStateProvider(mActivity);
        mTabCountProvider = new TabCountProvider();
        mTabThemeColorProvider = themeColorProvider;
        mTabThemeColorProvider.addThemeColorObserver(this);

        mAppThemeColorProvider = new AppThemeColorProvider(mActivity);

        mToolbar =
                new TopToolbarCoordinator(controlContainer, mActivity.findViewById(R.id.toolbar));
        mActionModeController = new ActionModeController(mActivity, mActionBarDelegate);
        mActionModeController.setCustomSelectionActionModeCallback(mToolbarActionModeCallback);

        mIdentityDiscController =
                new IdentityDiscController(activity, this, activity.getLifecycleDispatcher());

        mToolbar.setPaintInvalidator(invalidator);
        mActionModeController.setTabStripHeight(mToolbar.getTabStripHeight());
        mLocationBar = mToolbar.getLocationBar();
        mLocationBar.setToolbarDataProvider(mLocationBarModel);
        mLocationBar.addUrlFocusChangeListener(this);
        mLocationBar.setDefaultTextEditActionModeCallback(
                mActionModeController.getActionModeCallback());
        mLocationBar.initializeControls(new WindowDelegate(mActivity.getWindow()),
                mActivity.getWindowAndroid(), mActivity.getActivityTabProvider());
        mLocationBar.addUrlFocusChangeListener(mLocationBarFocusObserver);

        mToolbar.initialize(mLocationBarModel, this);
        mToolbar.addUrlExpansionObserver(activity.getStatusBarColorController());

        mOmniboxStartupMetrics = new OmniboxStartupMetrics(activity);

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                refreshSelectedTab();
            }

            @Override
            public void onTabStateInitialized() {
                mTabRestoreCompleted = true;
                handleTabRestoreCompleted();
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                mPreselectedTabId = Tab.INVALID_TAB_ID;
                refreshSelectedTab();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                refreshSelectedTab();
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                mLocationBar.setTitleToPageTitle();
                refreshSelectedTab();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                refreshSelectedTab();
            }

            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                refreshSelectedTab();
            }

            @Override
            public void tabRemoved(Tab tab) {
                refreshSelectedTab();
            }
        };

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                if (mLocationBarModel.getTab() == null) return;

                assert tab == mLocationBarModel.getTab();
                mLocationBar.updateStatusIcon();
                mLocationBar.setUrlToPageUrl();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                mLocationBar.setTitleToPageTitle();
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the SSL security state as a result of this notification as it will
                // sometimes be the only update we receive.
                updateTabLoadingState(true);

                // A URL update is a decent enough indicator that the toolbar widget is in
                // a stable state to capture its bitmap for use in fullscreen.
                mControlContainer.setReadyForBitmapCapture(true);
            }

            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                if (TextUtils.isEmpty(tab.getUrl())) return;
                mControlContainer.setReadyForBitmapCapture(true);
            }

            @Override
            public void onCrash(Tab tab) {
                updateTabLoadingState(false);
                updateButtonStatus();
                finishLoadProgress(false);
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                if (tab.isShowingErrorPage()) {
                    handleIPHForErrorPageShown(tab);
                    return;
                }

                // TODO(crbug.com/896476): Remove this.
                if (tab.isPreview()) {
                    // Some previews (like Client LoFi) are not fully decided until the page
                    // finishes loading. If this is a preview, update the security icon which will
                    // also update the verbose status view to make sure the "Lite" badge is
                    // displayed.
                    mLocationBar.updateStatusIcon();
                    PreviewsUma.recordLitePageAtLoadFinish(
                            PreviewsAndroidBridge.getInstance().getPreviewsType(
                                    tab.getWebContents()));
                }

                handleIPHForSuccessfulPageLoad(tab);
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateButtonStatus();
                updateTabLoadingState(true);
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                updateTabLoadingState(true);

                // If we made some progress, fast-forward to complete, otherwise just dismiss any
                // MINIMUM_LOAD_PROGRESS that had been set.
                if (tab.getProgress() > MINIMUM_LOAD_PROGRESS && tab.getProgress() < 1) {
                    updateLoadProgress(1);
                }
                finishLoadProgress(true);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;

                updateLoadProgress(progress);
            }

            @Override
            public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
                if (mFindToolbarManager != null) {
                    mFindToolbarManager.hideToolbar();
                }
            }

            @Override
            public void onContentChanged(Tab tab) {
                if (tab.isNativePage()) TabThemeColorHelper.get(tab).updateIfNeeded(false);
                mToolbar.onTabContentViewChanged();
                if (shouldShowCursorInLocationBar()) {
                    mLocationBar.showUrlBarCursorWithoutFocusAnimations();
                }
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                if (!didStartLoad) return;
                mLocationBar.updateLoadingState(true);
                if (didFinishLoad) {
                    mLoadProgressSimulator.start();
                }
            }

            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                NewTabPage ntp = mLocationBarModel.getNewTabPageForCurrentTab();
                if (ntp == null) return;
                if (!NewTabPage.isNTPUrl(params.getUrl())
                        && loadType != TabLoadStatus.PAGE_LOAD_FAILED) {
                    ntp.setUrlFocusAnimationsDisabled(true);
                    mToolbar.onTabOrModelChanged();
                }
            }

            private boolean hasPendingNonNtpNavigation(Tab tab) {
                WebContents webContents = tab.getWebContents();
                if (webContents == null) return false;

                NavigationController navigationController = webContents.getNavigationController();
                if (navigationController == null) return false;

                NavigationEntry pendingEntry = navigationController.getPendingEntry();
                if (pendingEntry == null) return false;

                return !NewTabPage.isNTPUrl(pendingEntry.getUrl());
            }

            @Override
            public void onContextualActionBarVisibilityChanged(Tab tab, boolean visible) {
                if (visible) RecordUserAction.record("MobileActionBarShown");
                ActionBar actionBar = mActionBarDelegate.getSupportActionBar();
                if (!visible && actionBar != null) actionBar.hide();
                if (mActivity.isTablet()) {
                    if (visible) {
                        mActionModeController.startShowAnimation();
                    } else {
                        mActionModeController.startHideAnimation();
                    }
                }
            }

            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigation) {
                if (!navigation.isInMainFrame()) return;
                // Update URL as soon as it becomes available when it's a new tab.
                // But we want to update only when it's a new tab. So we check whether the current
                // navigation entry is initial, meaning whether it has the same target URL as the
                // initial URL of the tab.
                if (tab.getWebContents() != null
                        && tab.getWebContents().getNavigationController() != null
                        && tab.getWebContents().getNavigationController().isInitialNavigation()) {
                    mLocationBar.setUrlToPageUrl();
                }

                if (navigation.isSameDocument()) return;
                // This event is used as the primary trigger for the progress bar because it
                // is the earliest indication that a load has started for a particular frame. In
                // the case of the progress bar, it should only traverse the screen a single time
                // per page load. So if this event states the main frame has started loading the
                // progress bar is started.

                if (NativePageFactory.isNativePageUrl(navigation.getUrl(), tab.isIncognito())) {
                    finishLoadProgress(false);
                    return;
                }

                mLoadProgressSimulator.cancel();
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()
                        && !navigation.isSameDocument()) {
                    mToolbar.onNavigatedToDifferentPage();
                }

                if (navigation.hasCommitted() && tab.isPreview()) {
                    // Some previews are not fully decided until the page commits. If this
                    // is a preview, update the security icon which will also update the verbose
                    // status view to make sure the "Lite" badge is displayed.
                    mLocationBar.updateStatusIcon();
                    PreviewsUma.recordLitePageAtCommit(
                            PreviewsAndroidBridge.getInstance().getPreviewsType(
                                    tab.getWebContents()),
                            navigation.isInMainFrame());
                }

                // If the load failed due to a different navigation, there is no need to reset the
                // location bar animations.
                if (navigation.errorCode() != 0 && navigation.isInMainFrame()
                        && !hasPendingNonNtpNavigation(tab)) {
                    NewTabPage ntp = mLocationBarModel.getNewTabPageForCurrentTab();
                    if (ntp == null) return;

                    ntp.setUrlFocusAnimationsDisabled(false);
                    mToolbar.onTabOrModelChanged();
                    if (mToolbar.getProgressBar() != null) mToolbar.getProgressBar().finish(false);
                }
            }

            @Override
            public void onNavigationEntriesDeleted(Tab tab) {
                if (tab == mLocationBarModel.getTab()) {
                    updateButtonStatus();
                }
            }

            private void handleIPHForSuccessfulPageLoad(final Tab tab) {
                if (mTextBubble != null) {
                    mTextBubble.dismiss();
                    mTextBubble = null;
                    return;
                }

                showDownloadPageTextBubble(tab, FeatureConstants.DOWNLOAD_PAGE_FEATURE);
                showTranslateMenuButtonTextBubble(
                        tab, FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE);
            }

            private void handleIPHForErrorPageShown(Tab tab) {
                if (!(mActivity instanceof ChromeTabbedActivity) || mActivity.isTablet()) {
                    return;
                }

                OfflinePageBridge bridge = OfflinePageBridge.getForProfile(tab.getProfile());
                if (bridge == null
                        || !bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents())) {
                    return;
                }

                Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
                tracker.notifyEvent(EventConstants.USER_HAS_SEEN_DINO);
            }
        };

        mBookmarksObserver = new BookmarkBridge.BookmarkModelObserver() {
            @Override
            public void bookmarkModelChanged() {
                updateBookmarkButtonStatus();
            }
        };

        mFindToolbarObserver = new FindToolbarObserver() {
            @Override
            public void onFindToolbarShown() {
                mToolbar.handleFindLocationBarStateChange(true);
                if (mControlsVisibilityDelegate != null) {
                    mFullscreenFindInPageToken =
                            mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                    mFullscreenFindInPageToken);
                }
            }

            @Override
            public void onFindToolbarHidden() {
                mToolbar.handleFindLocationBarStateChange(false);
                if (mControlsVisibilityDelegate != null) {
                    mControlsVisibilityDelegate.releasePersistentShowingToken(
                            mFullscreenFindInPageToken);
                }
            }
        };

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mToolbar.setTabSwitcherMode(true, showToolbar, false);
                updateButtonStatus();
            }

            @Override
            public void onOverviewModeStateChanged(boolean showTabSwitcherToolbar) {
                mToolbar.updateTabSwitcherToolbarState(showTabSwitcherToolbar);
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mToolbar.setTabSwitcherMode(false, showToolbar, delayAnimation);
                updateButtonStatus();
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mToolbar.onTabSwitcherTransitionFinished();
            }
        };

        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {
                mPreselectedTabId = tabId;
                refreshSelectedTab();

                if (mToolbar.setForceTextureCapture(true)) {
                    mControlContainer.invalidateBitmap();
                }
            }

            @Override
            public void onSceneChange(Layout layout) {
                mToolbar.setContentAttached(layout.shouldDisplayContentOverlay());
            }
        };

        mLoadProgressSimulator = new LoadProgressSimulator(this);

        mToolbar.setTabCountProvider(mTabCountProvider);
        mToolbar.setIncognitoStateProvider(mIncognitoStateProvider);
        mToolbar.setThemeColorProvider(
                mActivity.isTablet() ? mAppThemeColorProvider : mTabThemeColorProvider);
    }

    /**
     * Called when the app menu and related properties delegate are available.
     * @param appMenuCoordinator The coordinator for interacting with the menu.
     */
    public void onAppMenuInitialized(AppMenuCoordinator appMenuCoordinator) {
        AppMenuHandler appMenuHandler = appMenuCoordinator.getAppMenuHandler();
        MenuDelegatePhone menuDelegate = new MenuDelegatePhone() {
            @Override
            public void updateReloadButtonState(boolean isLoading) {
                if (mAppMenuPropertiesDelegate != null) {
                    mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
                    appMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
                }
            }
        };
        setMenuDelegatePhone(menuDelegate);
        setAppMenuHandler(appMenuHandler);
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    @Override
    public void onScrimVisibilityChanged(boolean visible) {
        if (visible) {
            mActivity.addViewObscuringAllTabs(mActivity.getScrim());
        } else {
            mActivity.removeViewObscuringAllTabs(mActivity.getScrim());
        }
    }

    @Override
    public void onScrimClick() {
        setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);
    }

    /**
     * @return  Whether the UrlBar currently has focus.
     */
    public boolean isUrlBarFocused() {
        return mLocationBar.isUrlBarFocused();
    }

    /**
     * Enable the bottom toolbar.
     */
    public void enableBottomToolbar() {
        // TODO(amaralp): Move creation of these listeners to bottom toolbar component.
        final OnClickListener homeButtonListener = v -> {
            recordBottomToolbarUseForIPH();
            openHomepage();
        };

        final OnClickListener searchAcceleratorListener = v -> {
            recordBottomToolbarUseForIPH();
            ACCELERATOR_BUTTON_TAP_ACTION.record();
            setUrlBarFocus(true, LocationBar.OmniboxFocusReason.ACCELERATOR_TAP);
        };

        final OnClickListener shareButtonListener = v -> {
            recordBottomToolbarUseForIPH();
            RecordUserAction.record("MobileBottomToolbarShareButton");
            Tab tab = null;
            Activity activity = null;
            boolean isIncognito = false;
            if (mTabModelSelector != null) {
                tab = mTabModelSelector.getCurrentTab();
                activity = tab.getActivity();
                isIncognito = tab.isIncognito();
            }
            mShareDelegate.share(tab, /*shareDirectly=*/false);
        };

        mBottomControlsCoordinator = new BottomControlsCoordinator(mActivity.getFullscreenManager(),
                mActivity.findViewById(R.id.bottom_controls_stub),
                mActivity.getActivityTabProvider(), homeButtonListener, searchAcceleratorListener,
                shareButtonListener,
                BottomTabSwitcherActionMenuCoordinator.createOnLongClickListener(
                        id -> mActivity.onOptionsItemSelected(id, null)),
                mAppThemeColorProvider);

        mIsBottomToolbarVisible = FeatureUtilities.isBottomToolbarEnabled()
                && (!FeatureUtilities.isAdaptiveToolbarEnabled()
                        || mActivity.getResources().getConfiguration().orientation
                                != Configuration.ORIENTATION_LANDSCAPE);
        mBottomControlsCoordinator.setBottomControlsVisible(mIsBottomToolbarVisible);
        mToolbar.onBottomToolbarVisibilityChanged(mIsBottomToolbarVisible);

        Toast.setGlobalExtraYOffset(mActivity.getResources().getDimensionPixelSize(
                FeatureUtilities.isLabeledBottomToolbarEnabled()
                        ? R.dimen.labeled_bottom_toolbar_height
                        : R.dimen.bottom_toolbar_height));
    }

    /** Record that homepage button was used for IPH reasons */
    private void recordToolbarUseForIPH(String toolbarIPHEvent) {
        if (mTabModelSelector != null && mTabModelSelector.getCurrentTab() != null) {
            Tab tab = mTabModelSelector.getCurrentTab();
            Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
            tracker.notifyEvent(toolbarIPHEvent);
        }
    }

    /** Record that the bottom toolbar was used for IPH reasons. */
    private void recordBottomToolbarUseForIPH() {
        recordToolbarUseForIPH(EventConstants.CHROME_DUET_USED_BOTTOM_TOOLBAR);
    }

    /**
     * Add bottom toolbar IPH tracking to an existing click listener.
     * @param listener The listener to add bottom toolbar tracking to.
     */
    private OnClickListener wrapBottomToolbarClickListenerForIPH(OnClickListener listener) {
        return (v) -> {
            recordBottomToolbarUseForIPH();
            listener.onClick(v);
        };
    }

    /**
     * @return Whether the bottom toolbar is visible.
     */
    public boolean isBottomToolbarVisible() {
        return mIsBottomToolbarVisible;
    }

    /**
     * @return The coordinator for the bottom toolbar if it exists.
     */
    public BottomControlsCoordinator getBottomToolbarCoordinator() {
        return mBottomControlsCoordinator;
    }

    // TODO(https://crbug.com/865801): Remove this IPH code from toolbar manager.
    private void showMenuIPHTextBubble(ChromeActivity activity, Tracker tracker, String featureName,
            @StringRes int stringId, @StringRes int accessibilityStringId, Integer highlightItemId,
            boolean circleHighlight) {
        ViewRectProvider rectProvider = new ViewRectProvider(getMenuButtonView());
        int yInsetPx = mActivity.getResources().getDimensionPixelOffset(
                R.dimen.text_bubble_menu_anchor_y_inset);
        rectProvider.setInsetPx(0, 0, 0, yInsetPx);
        mTextBubble = new TextBubble(
                mActivity, getMenuButtonView(), stringId, accessibilityStringId, rectProvider);
        mTextBubble.setDismissOnTouchInteraction(true);
        mTextBubble.addOnDismissListener(() -> {
            mHandler.postDelayed(() -> {
                tracker.dismissed(featureName);
                mAppMenuHandler.clearMenuHighlight();
            }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS);
        });
        mAppMenuHandler.setMenuHighlight(highlightItemId, circleHighlight);
        mTextBubble.show();
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     * @param tab The current tab.
     * @param featureName The associated feature name.
     */
    // TODO(https://crbug.com/865801): Remove feature specific IPH from toolbar manager.
    public void showDownloadPageTextBubble(final Tab tab, String featureName) {
        if (tab == null) return;

        // TODO(shaktisahu): Find out if the download menu button is enabled (crbug/712438).
        ChromeActivity activity = tab.getActivity();
        if (!(activity instanceof ChromeTabbedActivity) || activity.isTablet()
                || activity.isInOverviewMode() || !DownloadUtils.isAllowedToDownloadPage(tab)) {
            return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        showMenuIPHTextBubble(activity, tracker, featureName,
                R.string.iph_download_page_for_offline_usage_text,
                R.string.iph_download_page_for_offline_usage_accessibility_text,
                R.id.offline_page_id, true);

        // Record metrics if we show Download IPH after a screenshot of the page.
        ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
        ScreenshotTabObserver tabObserver =
                ScreenshotTabObserver.from(chromeActivity.getActivityTab());
        if (tabObserver != null) {
            tabObserver.onActionPerformedAfterScreenshot(
                    ScreenshotTabObserver.SCREENSHOT_ACTION_DOWNLOAD_IPH);
        }
    }

    /**
     * Show the translate manual trigger in-product-help bubble.
     * @param tab The current tab.
     * @param featureName The associated feature name.
     */
    // TODO(https://crbug.com/865801): Remove feature specific IPH from toolbar manager.
    public void showTranslateMenuButtonTextBubble(final Tab tab, String featureName) {
        if (tab == null) return;
        ChromeActivity activity = tab.getActivity();

        if (mAppMenuPropertiesDelegate == null || !TranslateUtils.canTranslateCurrentTab(tab)
                || !TranslateBridge.shouldShowManualTranslateIPH(tab)) {
            return;
        }
        // Find out if the help UI should appear.
        final Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        showMenuIPHTextBubble(activity, tracker, featureName,
                R.string.iph_translate_menu_button_text,
                R.string.iph_translate_menu_button_accessibility_text, R.id.translate_id, false);
    }

    /**
     * Initialize the manager with the components that had native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector           The selector that handles tab management.
     * @param controlsVisibilityDelegate The delegate to handle visibility of browser controls.
     * @param overviewModeBehavior       The overview mode manager.
     * @param layoutManager              A {@link LayoutManager} instance used to watch for scene
     *                                   changes.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            OverviewModeBehavior overviewModeBehavior, LayoutManager layoutManager,
            OnClickListener tabSwitcherClickHandler, OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler, OnClickListener customTabsBackClickHandler) {
        assert !mInitializedWithNative;

        mTabModelSelector = tabModelSelector;

        OnLongClickListener tabSwitcherLongClickHandler = null;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_SWITCHER_LONGPRESS_MENU)) {
            tabSwitcherLongClickHandler =
                    TabSwitcherActionMenuCoordinator.createOnLongClickListener(
                            (id) -> mActivity.onOptionsItemSelected(id, null));
        }

        mToolbar.initializeWithNative(tabModelSelector, controlsVisibilityDelegate, layoutManager,
                tabSwitcherClickHandler, tabSwitcherLongClickHandler, newTabClickHandler,
                bookmarkClickHandler, customTabsBackClickHandler);

        mToolbar.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {}

            @Override
            public void onViewAttachedToWindow(View v) {
                // As we have only just registered for notifications, any that were sent prior
                // to this may have been missed. Calling refreshSelectedTab in case we missed
                // the initial selection notification.
                refreshSelectedTab();
            }
        });

        mLocationBarModel.initializeWithNative();
        mLocationBarModel.setShouldShowOmniboxInOverviewMode(
                FeatureUtilities.isStartSurfaceEnabled());

        assert controlsVisibilityDelegate != null;
        mControlsVisibilityDelegate = controlsVisibilityDelegate;

        mNativeLibraryReady = false;

        if (overviewModeBehavior != null) {
            mOverviewModeBehavior = overviewModeBehavior;
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            mAppThemeColorProvider.setOverviewModeBehavior(mOverviewModeBehavior);
            mLocationBarModel.setOverviewModeBehavior(mOverviewModeBehavior);
        }

        if (layoutManager != null) {
            mLayoutManager = layoutManager;
            mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
        }

        if (mBottomControlsCoordinator != null) {
            final OnClickListener closeTabsClickListener = v -> {
                recordBottomToolbarUseForIPH();
                final boolean isIncognito = mTabModelSelector.isIncognitoSelected();
                if (isIncognito) {
                    RecordUserAction.record("MobileToolbarCloseAllIncognitoTabsButtonTap");
                } else {
                    RecordUserAction.record("MobileToolbarCloseAllRegularTabsButtonTap");
                }

                mTabModelSelector.getModel(isIncognito).closeAllTabs();
            };
            if (mAppMenuButtonHelper != null) {
                mAppMenuButtonHelper.setOnClickRunnable(() -> recordBottomToolbarUseForIPH());
            }
            mBottomControlsCoordinator.initializeWithNative(mActivity,
                    mActivity.getCompositorViewHolder().getResourceManager(),
                    mActivity.getCompositorViewHolder().getLayoutManager(),
                    wrapBottomToolbarClickListenerForIPH(tabSwitcherClickHandler),
                    wrapBottomToolbarClickListenerForIPH(newTabClickHandler),
                    closeTabsClickListener, mAppMenuButtonHelper, mOverviewModeBehavior,
                    mActivity.getWindowAndroid(), mTabCountProvider, mIncognitoStateProvider,
                    mActivity.findViewById(R.id.control_container));

            // Allow the bottom toolbar to be focused in accessibility after the top toolbar.
            ApiCompatibilityUtils.setAccessibilityTraversalBefore(
                    mLocationBar.getContainerView(), R.id.bottom_toolbar);
        }

        onNativeLibraryReady();
        mInitializedWithNative = true;
        if (mOnInitializedRunnable != null) {
            mOnInitializedRunnable.run();
            mOnInitializedRunnable = null;
        }
    }

    /**
     * Set the {@link FindToolbarManager}.
     * @param findToolbarManager The manager for find in page.
     */
    public void setFindToolbarManager(FindToolbarManager findToolbarManager) {
        mFindToolbarManager = findToolbarManager;
        mFindToolbarManager.addObserver(mFindToolbarObserver);
    }

    /**
     * Show the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void showAppMenuUpdateBadge() {
        mToolbar.showAppMenuUpdateBadge();
        if (mBottomControlsCoordinator != null) {
            mBottomControlsCoordinator.showAppMenuUpdateBadge();
        }
    }

    /**
     * Remove the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void removeAppMenuUpdateBadge(boolean animate) {
        mToolbar.removeAppMenuUpdateBadge(animate);
        if (mBottomControlsCoordinator != null) {
            mBottomControlsCoordinator.removeAppMenuUpdateBadge();
        }
    }

    /**
     * @return Whether the badge is showing (either in the top or bottom toolbar).
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        if (mBottomControlsCoordinator != null
                && mBottomControlsCoordinator.isShowingAppMenuUpdateBadge()) {
            return true;
        }
        return mToolbar.isShowingAppMenuUpdateBadge();
    }

    /**
     * Enable the experimental toolbar button.
     * @param onClickListener The {@link OnClickListener} to be called when the button is clicked.
     * @param image The drawable to display for the button.
     * @param contentDescriptionResId The resource id of the content description for the button.
     */
    public void enableExperimentalButton(OnClickListener onClickListener, Drawable image,
            @StringRes int contentDescriptionResId) {
        mToolbar.enableExperimentalButton(onClickListener, image, contentDescriptionResId);
    }

    /**
     * Disable the experimental toolbar button.
     */
    public void disableExperimentalButton() {
        mToolbar.disableExperimentalButton();
    }

    /**
     * Updates image displayed on experimental button.
     */
    public void updateExperimentalButtonImage(Drawable image) {
        mToolbar.updateExperimentalButtonImage(image);
    }

    /**
     * Displays in-product help for experimental button.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param dismissedCallback The callback that will be called when in-product help is dismissed.
     */
    public void showIPHOnExperimentalButton(@StringRes int stringId,
            @StringRes int accessibilityStringId, Runnable dismissedCallback) {
        mToolbar.showIPHOnExperimentalButton(stringId, accessibilityStringId, dismissedCallback);
    }

    /**
     * @return The bookmarks bridge.
     */
    public BookmarkBridge getBookmarkBridge() {
        return mBookmarkBridge;
    }

    /**
     * @return An {@link ObservableSupplier} that supplies the {@link BookmarksBridge}.
     */
    public ObservableSupplier<BookmarkBridge> getBookmarkBridgeSupplier() {
        return mBookmarkBridgeSupplier;
    }

    /**
     * @return The toolbar interface that this manager handles.
     */
    public Toolbar getToolbar() {
        return mToolbar;
    }

    /**
     * @return The callback for toolbar action mode controller.
     */
    public ToolbarActionModeCallback getActionModeControllerCallback() {
        return mToolbarActionModeCallback;
    }

    /**
     * @return Whether the UI has been initialized.
     */
    public boolean isInitialized() {
        return mInitializedWithNative;
    }

    @Override
    public @Nullable View getMenuButtonView() {
        if (mBottomControlsCoordinator != null && isBottomToolbarVisible()) {
            return mBottomControlsCoordinator.getMenuButtonWrapper().getImageButton();
        }
        return mToolbar.getMenuButton();
    }

    @Override
    public boolean isMenuFromBottom() {
        return isBottomToolbarVisible();
    }

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    public View getSecurityIconView() {
        return mLocationBar.getSecurityIconView();
    }

    /**
     * Adds a custom action button to the {@link Toolbar}, if it is supported.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     * @see #updateCustomActionButton
     */
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        mToolbar.addCustomActionButton(drawable, description, listener);
    }

    /**
     * Updates the visual appearance of a custom action button in the {@link Toolbar},
     * if it is supported.
     * @param index The index of the button to update.
     * @param drawable The {@link Drawable} to use as the background for the button.
     * @param description The content description for the custom action button.
     * @see #addCustomActionButton
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbar.updateCustomActionButton(index, drawable, description);
    }

    /**
     * Call to tear down all of the toolbar dependencies.
     */
    public void destroy() {
        if (mInitializedWithNative) {
            mFindToolbarManager.removeObserver(mFindToolbarObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            for (TabModel model : mTabModelSelector.getModels()) {
                model.removeObserver(mTabModelObserver);
            }
        }
        if (mBookmarkBridge != null) {
            mBookmarkBridge.destroy();
            mBookmarkBridge = null;
            mBookmarkBridgeSupplier.set(null);
        }
        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
            mTemplateUrlObserver = null;
        }
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
            mOverviewModeBehavior = null;
        }
        if (mLayoutManager != null) {
            mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
            mLayoutManager = null;
        }

        if (mBottomControlsCoordinator != null) {
            mBottomControlsCoordinator.destroy();
            mBottomControlsCoordinator = null;
        }

        if (mOmniboxStartupMetrics != null) {
            // Record the histogram before destroying, if we have the data.
            if (mInitializedWithNative) mOmniboxStartupMetrics.maybeRecordHistograms();
            mOmniboxStartupMetrics.destroy();
            mOmniboxStartupMetrics = null;
        }

        if (mLocationBar != null) {
            mLocationBar.removeUrlFocusChangeListener(this);
        }

        mToolbar.removeUrlExpansionObserver(mActivity.getStatusBarColorController());
        mToolbar.destroy();

        if (mTabObserver != null) {
            Tab currentTab = mLocationBarModel.getTab();
            if (currentTab != null) currentTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }

        mIncognitoStateProvider.destroy();
        mTabCountProvider.destroy();

        mIdentityDiscController.destroy();
        mLocationBarModel.destroy();
        mHandler.removeCallbacksAndMessages(null); // Cancel delayed tasks.
        if (mLocationBar != null) {
            mLocationBar.removeUrlFocusChangeListener(mLocationBarFocusObserver);
            mLocationBarFocusObserver = null;
        }

        if (mTabThemeColorProvider != null) mTabThemeColorProvider.removeThemeColorObserver(this);
        if (mAppThemeColorProvider != null) mAppThemeColorProvider.destroy();
        mOrientationEventListener.disable();
        mOrientationEventListener = null;
    }

    /**
     * Called when the orientation of the activity has changed.
     */
    public void onOrientationChange() {
        if (mActionModeController != null) mActionModeController.showControlsOnOrientationChange();

        if (mBottomControlsCoordinator != null && FeatureUtilities.isBottomToolbarEnabled()
                && FeatureUtilities.isAdaptiveToolbarEnabled()) {
            mIsBottomToolbarVisible = mActivity.getResources().getConfiguration().orientation
                    != Configuration.ORIENTATION_LANDSCAPE;
            mToolbar.onBottomToolbarVisibilityChanged(mIsBottomToolbarVisible);
            mBottomControlsCoordinator.setBottomControlsVisible(mIsBottomToolbarVisible);
            if (mAppMenuButtonHelper != null) {
                mAppMenuButtonHelper.setMenuShowsFromBottom(mIsBottomToolbarVisible);
            }
            mIdentityDiscController.updateButtonState();
        }
    }

    /**
     * Called when the accessibility enabled state changes.
     * @param enabled Whether accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mToolbar.onAccessibilityStatusChanged(enabled);
    }

    private void registerTemplateUrlObserver() {
        final TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        assert mTemplateUrlObserver == null;
        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            private TemplateUrl mSearchEngine =
                    templateUrlService.getDefaultSearchEngineTemplateUrl();

            @Override
            public void onTemplateURLServiceChanged() {
                TemplateUrl searchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
                if ((mSearchEngine == null && searchEngine == null)
                        || (mSearchEngine != null && mSearchEngine.equals(searchEngine))) {
                    return;
                }

                mSearchEngine = searchEngine;
                mLocationBar.updateSearchEngineStatusIcon(
                        SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                                mLocationBarModel.isIncognito()),
                        TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                        SearchEngineLogoUtils.getSearchLogoUrl());
                mToolbar.onDefaultSearchEngineChanged();
            }
        };
        templateUrlService.addObserver(mTemplateUrlObserver);

        // Force an update once to populate initial data.
        mLocationBar.updateSearchEngineStatusIcon(
                SearchEngineLogoUtils.shouldShowSearchEngineLogo(mLocationBarModel.isIncognito()),
                TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle(),
                SearchEngineLogoUtils.getSearchLogoUrl());
    }

    private void onNativeLibraryReady() {
        mNativeLibraryReady = true;

        final TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        TemplateUrlService.LoadListener mTemplateServiceLoadListener =
                new TemplateUrlService.LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        registerTemplateUrlObserver();
                        templateUrlService.unregisterLoadListener(this);
                    }
                };
        templateUrlService.registerLoadListener(mTemplateServiceLoadListener);
        if (templateUrlService.isLoaded()) {
            mTemplateServiceLoadListener.onTemplateUrlServiceLoaded();
        } else {
            templateUrlService.load();
        }

        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        for (TabModel model : mTabModelSelector.getModels()) model.addObserver(mTabModelObserver);

        refreshSelectedTab();
        if (mTabModelSelector.isTabStateInitialized()) mTabRestoreCompleted = true;
        handleTabRestoreCompleted();
        mTabCountProvider.setTabModelSelector(mTabModelSelector);
        mIncognitoStateProvider.setTabModelSelector(mTabModelSelector);
        mAppThemeColorProvider.setIncognitoStateProvider(mIncognitoStateProvider);
    }

    private void handleTabRestoreCompleted() {
        if (!mTabRestoreCompleted || !mNativeLibraryReady) return;
        mToolbar.onStateRestored();
    }

    /**
     * Sets the handler for any special case handling related with the menu button.
     * @param appMenuHandler The handler to be used.
     */
    private void setAppMenuHandler(AppMenuHandler appMenuHandler) {
        mAppMenuHandler = appMenuHandler;
        mAppMenuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible) {
                    // Defocus here to avoid handling focus in multiple places, e.g., when the
                    // forward button is pressed. (see crbug.com/414219)
                    setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

                    if (!mActivity.isInOverviewMode() && isShowingAppMenuUpdateBadge()) {
                        // The app menu badge should be removed the first time the menu is opened.
                        removeAppMenuUpdateBadge(true);
                        mActivity.getCompositorViewHolder().requestRender();
                    }
                }

                if (mControlsVisibilityDelegate != null) {
                    if (isVisible) {
                        mFullscreenMenuToken =
                                mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                        mFullscreenMenuToken);
                    } else {
                        mControlsVisibilityDelegate.releasePersistentShowingToken(
                                mFullscreenMenuToken);
                    }
                }

                MenuButton menuButton = getMenuButtonWrapper();
                if (isVisible && menuButton != null && menuButton.isShowingAppMenuUpdateBadge()) {
                    UpdateMenuItemHelper.getInstance().onMenuButtonClicked();
                }
            }

            @Override
            public void onMenuHighlightChanged(boolean highlighting) {
                final MenuButton menuButton = getMenuButtonWrapper();
                if (menuButton != null) menuButton.setMenuButtonHighlight(highlighting);

                if (mControlsVisibilityDelegate == null) return;
                if (highlighting) {
                    mFullscreenHighlightToken =
                            mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                    mFullscreenHighlightToken);
                } else {
                    mControlsVisibilityDelegate.releasePersistentShowingToken(
                            mFullscreenHighlightToken);
                }
            }
        });
        mAppMenuButtonHelper = mAppMenuHandler.createAppMenuButtonHelper();
        mAppMenuButtonHelper.setMenuShowsFromBottom(isBottomToolbarVisible());
        mAppMenuButtonHelper.setOnAppMenuShownListener(() -> {
            RecordUserAction.record("MobileToolbarShowMenu");
            if (isBottomToolbarVisible()) {
                RecordUserAction.record("MobileBottomToolbarShowMenu");
            } else {
                RecordUserAction.record("MobileTopToolbarShowMenu");
            }
            mToolbar.onMenuShown();

            // Assume data saver footer is shown only if data reduction proxy is enabled and
            // Chrome home is not
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
                tracker.notifyEvent(EventConstants.OVERFLOW_OPENED_WITH_DATA_SAVER_SHOWN);
            }
        });
        mToolbar.setAppMenuButtonHelper(mAppMenuButtonHelper);
    }

    @Nullable
    private MenuButton getMenuButtonWrapper() {
        if (mBottomControlsCoordinator != null) {
            return mBottomControlsCoordinator.getMenuButtonWrapper();
        }

        return mToolbar.getMenuButtonWrapper();
    }

    /**
     * Set the delegate that will handle updates from toolbar driven state changes.
     * @param menuDelegatePhone The menu delegate to be updated (only applicable to phones).
     */
    private void setMenuDelegatePhone(MenuDelegatePhone menuDelegatePhone) {
        mMenuDelegatePhone = menuDelegatePhone;
    }

    @Override
    public @ChromeTabbedActivity.BackPressedResult Integer back() {
        if (mBottomControlsCoordinator != null && mBottomControlsCoordinator.onBackPressed()) {
            return ChromeTabbedActivity.BackPressedResult.EXITED_TAB_GROUP_DIALOG;
        }

        Tab tab = mLocationBarModel.getTab();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
            updateButtonStatus();
            return ChromeTabbedActivity.BackPressedResult.NAVIGATED_BACK;
        }
        return null;
    }

    @Override
    public boolean forward() {
        Tab tab = mLocationBarModel.getTab();
        if (tab != null && tab.canGoForward()) {
            tab.goForward();
            updateButtonStatus();
            return true;
        }
        return false;
    }

    @Override
    public void stopOrReloadCurrentTab() {
        Tab currentTab = mLocationBarModel.getTab();
        if (currentTab != null) {
            if (currentTab.isLoading()) {
                currentTab.stopLoading();
                RecordUserAction.record("MobileToolbarStop");
            } else {
                currentTab.reload();
                RecordUserAction.record("MobileToolbarReload");
            }
        }
        updateButtonStatus();
    }

    @Override
    public void openHomepage() {
        RecordUserAction.record("Home");

        if (isBottomToolbarVisible()) {
            RecordUserAction.record("MobileBottomToolbarHomeButton");
        } else {
            RecordUserAction.record("MobileTopToolbarHomeButton");
        }

        Tab currentTab = mLocationBarModel.getTab();
        if (currentTab == null) return;
        String homePageUrl = HomepageManager.getHomepageUri();
        if (TextUtils.isEmpty(homePageUrl)) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        boolean is_chrome_internal =
                homePageUrl.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                || homePageUrl.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || homePageUrl.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX);
        RecordHistogram.recordBooleanHistogram(
                "Navigation.Home.IsChromeInternal", is_chrome_internal);

        recordToolbarUseForIPH(EventConstants.HOMEPAGE_BUTTON_CLICKED);
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mToolbar.onUrlFocusChange(hasFocus);

        if (hasFocus) mOmniboxStartupMetrics.onUrlBarFocused();

        if (mFindToolbarManager != null && hasFocus) mFindToolbarManager.hideToolbar();

        if (mControlsVisibilityDelegate == null) return;
        if (hasFocus) {
            mFullscreenFocusToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenFocusToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenFocusToken);
        }

        mUrlFocusChangedCallback.onResult(hasFocus);
    }

    /**
     * Updates the primary color used by the model to the given color.
     * @param color The primary color for the current tab.
     * @param shouldAnimate Whether the change of color should be animated.
     */
    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        if (!mShouldUpdateToolbarPrimaryColor) return;

        boolean colorChanged = mCurrentThemeColor != color;
        if (!colorChanged) return;

        mCurrentThemeColor = color;
        mLocationBarModel.setPrimaryColor(color);
        mToolbar.onPrimaryColorChanged(shouldAnimate);
    }

    /**
     * @param shouldUpdate Whether we should be updating the toolbar primary color based on updates
     *                     from the Tab.
     */
    public void setShouldUpdateToolbarPrimaryColor(boolean shouldUpdate) {
        mShouldUpdateToolbarPrimaryColor = shouldUpdate;
    }

    /**
     * @return Whether we should be updating the toolbar primary color based on updates from the
     * Tab.
     */
    public boolean getShouldUpdateToolbarPrimaryColor() {
        return mShouldUpdateToolbarPrimaryColor;
    }

    /**
     * @return The primary toolbar color.
     */
    public int getPrimaryColor() {
        return mLocationBarModel.getPrimaryColor();
    }

    /**
     * Gets the visibility of the Toolbar shadow.
     * @return One of View.VISIBLE, View.INVISIBLE, or View.GONE.
     */
    public int getToolbarShadowVisibility() {
        View toolbarShadow = mControlContainer.findViewById(R.id.toolbar_shadow);
        return (toolbarShadow != null) ? toolbarShadow.getVisibility() : View.GONE;
    }

    /**
     * Sets the visibility of the Toolbar shadow.
     */
    public void setToolbarShadowVisibility(int visibility) {
        View toolbarShadow = mControlContainer.findViewById(R.id.toolbar_shadow);
        if (toolbarShadow != null) toolbarShadow.setVisibility(visibility);
    }

    /**
     * Gets the visibility of the Toolbar.
     * @return One of View.VISIBLE, View.INVISIBLE, or View.GONE.
     */
    public int getToolbarVisibility() {
        View toolbar = getToolbarView();
        return (toolbar != null) ? toolbar.getVisibility() : View.GONE;
    }

    /**
     * Sets the visibility of the Toolbar.
     */
    public void setToolbarVisibility(int visibility) {
        View toolbar = getToolbarView();
        if (toolbar != null) toolbar.setVisibility(visibility);
    }

    /**
     * Sets the top margin for the control container.
     * @param margin The margin in pixels.
     */
    public void setControlContainerTopMargin(int margin) {
        final ViewGroup.MarginLayoutParams layoutParams =
                ((ViewGroup.MarginLayoutParams) mControlContainer.getLayoutParams());
        layoutParams.topMargin = margin;
        mControlContainer.setLayoutParams(layoutParams);
    }

    /**
     * Gets the Toolbar view.
     */
    @Nullable
    public View getToolbarView() {
        return mControlContainer.findViewById(R.id.toolbar);
    }

    /**
     * Sets the drawable that the close button shows, or hides it if {@code drawable} is
     * {@code null}.
     */
    public void setCloseButtonDrawable(Drawable drawable) {
        mToolbar.setCloseButtonImageResource(drawable);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        mToolbar.setShowTitle(showTitle);
    }

    /**
     * @see TopToolbarCoordinator#setUrlBarHidden(boolean)
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbar.setUrlBarHidden(hidden);
    }

    /**
     * @see TopToolbarCoordinator#getContentPublisher()
     */
    public String getContentPublisher() {
        return mToolbar.getContentPublisher();
    }

    /**
     * Focuses or unfocuses the URL bar.
     *
     * If you request focus and the UrlBar was already focused, this will select all of the text.
     *
     * @param focused Whether URL bar should be focused.
     * @param reason The given reason.
     */
    public void setUrlBarFocus(boolean focused, @LocationBar.OmniboxFocusReason int reason) {
        if (!isInitialized()) return;
        boolean wasFocused = mLocationBar.isUrlBarFocused();
        mLocationBar.setUrlBarFocus(focused, null, reason);
        if (wasFocused && focused) {
            mLocationBar.selectAll();
        }
    }

    /**
     * See {@link #setUrlBarFocus}, but if native is not loaded it will queue the request instead
     * of dropping it.
     */
    public void setUrlBarFocusOnceNativeInitialized(
            boolean focused, @LocationBar.OmniboxFocusReason int reason) {
        if (isInitialized()) {
            setUrlBarFocus(focused, reason);
            return;
        }

        if (focused) {
            // Remember requests to focus the Url bar and replay them once native has been
            // initialized. This is important for the Launch to Incognito Tab flow (see
            // IncognitoTabLauncher.
            mOnInitializedRunnable = () -> {
                setUrlBarFocus(focused, reason);
            };
        } else {
            mOnInitializedRunnable = null;
        }
    }

    /**
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     */
    public void setImmersiveModeManager(ImmersiveModeManager immersiveModeManager) {
        if (mBottomControlsCoordinator != null) {
            mBottomControlsCoordinator.setImmersiveModeManager(immersiveModeManager);
        }
    }

    /**
     * Reverts any pending edits of the location bar and reset to the page state.  This does not
     * change the focus state of the location bar.
     */
    public void revertLocationBarChanges() {
        mLocationBar.revertChanges();
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     * @param activityCreationTimeMs The time of creation for the activity this toolbar belongs to.
     * @param activityName Simple class name for the activity this toolbar belongs to.
     */
    public void onDeferredStartup(final long activityCreationTimeMs, final String activityName) {
        recordStartupHistograms(activityCreationTimeMs, activityName);
        mLocationBar.onDeferredStartup();
    }

    /**
     * Record histograms covering Chrome startup.
     * This method will collect metrics no sooner than RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS since
     * Activity creation to ensure availability of collected data.
     *
     * Histograms will not be collected if Chrome is destroyed before the above timeout passed.
     */
    private void recordStartupHistograms(
            final long activityCreationTimeMs, final String activityName) {
        // Schedule call to self if minimum time since activity creation has not yet passed.
        long elapsedTime = SystemClock.elapsedRealtime() - activityCreationTimeMs;
        if (elapsedTime < RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS) {
            // clang-format off
            mHandler.postDelayed(
                    () -> recordStartupHistograms(activityCreationTimeMs, activityName),
                    RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS - elapsedTime);
            // clang-format on
            return;
        }

        RecordHistogram.recordTimesHistogram("MobileStartup.ToolbarFirstDrawTime2." + activityName,
                mToolbar.getFirstDrawTime() - activityCreationTimeMs);

        mOmniboxStartupMetrics.maybeRecordHistograms();
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        if (isInitialized()) mToolbar.finishAnimations();
    }

    /**
     * See {@link LocationBar#updateVisualsForState()}
     */
    public void updateLocationBarVisualsForState() {
        mLocationBar.updateVisualsForState();
    }

    /**
     * Updates the current button states and calls appropriate abstract visibility methods, giving
     * inheriting classes the chance to update the button visuals as well.
     */
    private void updateButtonStatus() {
        Tab currentTab = mLocationBarModel.getTab();
        boolean tabCrashed = currentTab != null && SadTab.isShowing(currentTab);

        mToolbar.updateButtonVisibility();
        mToolbar.updateBackButtonVisibility(currentTab != null && currentTab.canGoBack());
        mToolbar.updateForwardButtonVisibility(currentTab != null && currentTab.canGoForward());
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();
        if (mToolbar.getMenuButtonWrapper() != null && !isBottomToolbarVisible()) {
            mToolbar.getMenuButtonWrapper().setVisibility(View.VISIBLE);
        }
        mIdentityDiscController.updateButtonState(
                mLocationBarModel.getNewTabPageForCurrentTab() != null);
    }

    private void updateBookmarkButtonStatus() {
        Tab currentTab = mLocationBarModel.getTab();
        boolean isBookmarked = currentTab != null && BookmarkBridge.hasBookmarkIdForTab(currentTab);
        boolean editingAllowed = currentTab == null || mBookmarkBridge == null
                || mBookmarkBridge.isEditBookmarksEnabled();
        mToolbar.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    private void updateReloadState(boolean tabCrashed) {
        Tab currentTab = mLocationBarModel.getTab();
        boolean isLoading = false;
        if (!tabCrashed) {
            isLoading = (currentTab != null && currentTab.isLoading()) || !mNativeLibraryReady;
        }
        mToolbar.updateReloadButtonVisibility(isLoading);
        if (mMenuDelegatePhone != null) mMenuDelegatePhone.updateReloadButtonState(isLoading);
    }

    /**
     * Triggered when the selected tab has changed.
     */
    private void refreshSelectedTab() {
        Tab tab = null;
        if (mPreselectedTabId != Tab.INVALID_TAB_ID) {
            tab = mTabModelSelector.getTabById(mPreselectedTabId);
        }
        if (tab == null) tab = mTabModelSelector.getCurrentTab();

        boolean wasIncognito = mLocationBarModel.isIncognito();
        Tab previousTab = mLocationBarModel.getTab();

        boolean isIncognito =
                tab != null ? tab.isIncognito() : mTabModelSelector.isIncognitoSelected();
        mLocationBarModel.setTab(tab, isIncognito);

        updateCurrentTabDisplayStatus();

        // This method is called prior to action mode destroy callback for incognito <-> normal
        // tab switch. Makes sure the action mode toolbar is hidden before selecting the new tab.
        if (previousTab != null && wasIncognito != isIncognito && mActivity.isTablet()) {
            mActionModeController.startHideAnimation();
        }
        if (previousTab != tab || wasIncognito != isIncognito) {
            if (previousTab != tab) {
                if (previousTab != null) {
                    previousTab.removeObserver(mTabObserver);
                }
                if (tab != null) tab.addObserver(mTabObserver);
            }

            int defaultPrimaryColor =
                    ChromeColors.getDefaultThemeColor(mActivity.getResources(), isIncognito);
            int primaryColor =
                    tab != null ? TabThemeColorHelper.getColor(tab) : defaultPrimaryColor;
            onThemeColorChanged(primaryColor, false);

            mToolbar.onTabOrModelChanged();

            if (tab != null && tab.getWebContents() != null
                    && tab.getWebContents().isLoadingToDifferentDocument()) {
                mToolbar.onNavigatedToDifferentPage();
            }

            // Ensure the URL bar loses focus if the tab it was interacting with is changed from
            // underneath it.
            setUrlBarFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

            // Place the cursor in the Omnibox if applicable.  We always clear the focus above to
            // ensure the shield placed over the content is dismissed when switching tabs.  But if
            // needed, we will refocus the omnibox and make the cursor visible here.
            if (shouldShowCursorInLocationBar()) {
                mLocationBar.showUrlBarCursorWithoutFocusAnimations();
            }
        }

        Profile profile = mTabModelSelector.getModel(isIncognito).getProfile();

        if (mCurrentProfile != profile) {
            if (mBookmarkBridge != null) {
                mBookmarkBridge.destroy();
                mBookmarkBridge = null;
            }
            if (profile != null) {
                mBookmarkBridge = new BookmarkBridge(profile);
                mBookmarkBridge.addObserver(mBookmarksObserver);
                mLocationBar.setAutocompleteProfile(profile);
                mLocationBar.setShowIconsWhenUrlFocused(
                        SearchEngineLogoUtils.shouldShowSearchEngineLogo(
                                mLocationBarModel.isIncognito()));
            }
            mCurrentProfile = profile;
            mBookmarkBridgeSupplier.set(mBookmarkBridge);
        }

        updateButtonStatus();
    }

    private void updateCurrentTabDisplayStatus() {
        Tab tab = mLocationBarModel.getTab();
        mLocationBar.setUrlToPageUrl();

        updateTabLoadingState(true);

        if (tab == null) {
            finishLoadProgress(false);
            return;
        }

        mLoadProgressSimulator.cancel();

        if (tab.isLoading()) {
            if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) {
                finishLoadProgress(false);
            } else {
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }
        } else {
            finishLoadProgress(false);
        }
    }

    private void updateTabLoadingState(boolean updateUrl) {
        mLocationBar.updateLoadingState(updateUrl);
        if (updateUrl) updateButtonStatus();
    }

    private void updateLoadProgress(float progress) {
        // If it's a native page, progress bar is already hidden or being hidden, so don't update
        // the value.
        // TODO(kkimlabs): Investigate back/forward navigation with native page & web content and
        //                 figure out the correct progress bar presentation.
        Tab tab = mLocationBarModel.getTab();
        if (tab == null || NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) {
            return;
        }

        progress = Math.max(progress, MINIMUM_LOAD_PROGRESS);
        mToolbar.setLoadProgress(progress);
        if (MathUtils.areFloatsEqual(progress, 1)) finishLoadProgress(true);
    }

    private void finishLoadProgress(boolean delayed) {
        mLoadProgressSimulator.cancel();
        mToolbar.finishLoadProgress(delayed);
    }

    /**
     * Only start showing the progress bar if it is not already started.
     */
    private void startLoadProgress() {
        if (mToolbar.isProgressStarted()) return;
        mToolbar.startLoadProgress();
    }

    /**
     * @param enabled Whether the progress bar is enabled.
     */
    public void setProgressBarEnabled(boolean enabled) {
        mToolbar.setProgressBarEnabled(enabled);
    }

    /**
     * @param anchor The view to use as an anchor.
     */
    public void setProgressBarAnchorView(@Nullable View anchor) {
        mToolbar.setProgressBarAnchorView(anchor);
    }

    /**
     * @return The {@link FakeboxDelegate}.
     */
    public FakeboxDelegate getFakeboxDelegate() {
        // TODO(crbug.com/1000295): Split fakebox component out of ntp package.
        return mLocationBar;
    }

    private boolean shouldShowCursorInLocationBar() {
        Tab tab = mLocationBarModel.getTab();
        if (tab == null) return false;
        NativePage nativePage = tab.getNativePage();
        if (!(nativePage instanceof NewTabPage) && !(nativePage instanceof IncognitoNewTabPage)) {
            return false;
        }

        return mActivity.isTablet()
                && mActivity.getResources().getConfiguration().keyboard
                == Configuration.KEYBOARD_QWERTY;
    }

    private static class LoadProgressSimulator {
        private static final int MSG_ID_UPDATE_PROGRESS = 1;

        private static final float PROGRESS_INCREMENT = 0.1f;
        private static final int PROGRESS_INCREMENT_DELAY_MS = 10;

        private final ToolbarManager mToolbarManager;
        private final Handler mHandler;

        private float mProgress;

        public LoadProgressSimulator(ToolbarManager toolbar) {
            mToolbarManager = toolbar;
            mHandler = new Handler(Looper.getMainLooper()) {
                @Override
                public void handleMessage(Message msg) {
                    assert msg.what == MSG_ID_UPDATE_PROGRESS;
                    mProgress = Math.min(1, mProgress += PROGRESS_INCREMENT);
                    mToolbarManager.updateLoadProgress(mProgress);

                    if (MathUtils.areFloatsEqual(mProgress, 1)) {
                        mToolbarManager.mToolbar.finishLoadProgress(true);
                        return;
                    }
                    sendEmptyMessageDelayed(MSG_ID_UPDATE_PROGRESS, PROGRESS_INCREMENT_DELAY_MS);
                }
            };
        }

        /**
         * Start simulating load progress from a baseline of 0.
         */
        public void start() {
            mProgress = 0;
            mToolbarManager.mToolbar.startLoadProgress();
            mToolbarManager.updateLoadProgress(mProgress);
            mHandler.sendEmptyMessage(MSG_ID_UPDATE_PROGRESS);
        }

        /**
         * Cancels simulating load progress.
         */
        public void cancel() {
            mHandler.removeMessages(MSG_ID_UPDATE_PROGRESS);
        }
    }

    /** Return the location bar model for testing purposes. */
    @VisibleForTesting
    public LocationBarModel getLocationBarModelForTesting() {
        return mLocationBarModel;
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    @VisibleForTesting
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbar.getToolbarLayoutForTesting();
    }
}
