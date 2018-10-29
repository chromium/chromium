// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.ActionBar;
import android.text.TextUtils;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnClickListener;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.CachedMetrics.ActionEvent;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.metrics.OmniboxStartupMetrics;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.QueryInOmnibox;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager.HomepageStateListener;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrl;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarButtonSlotData.ToolbarButtonData;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.ViewHighlighter;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.widget.findinpage.FindToolbarObserver;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Contains logic for managing the toolbar visual component.  This class manages the interactions
 * with the rest of the application to ensure the toolbar is always visually up to date.
 */
public class ToolbarManager implements ToolbarTabController, UrlFocusChangeListener {

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

    /** A means of tracking which mechanism is being used to focus the omnibox. */
    @IntDef({OmniboxFocusReason.OMNIBOX_TAP, OmniboxFocusReason.OMNIBOX_LONG_PRESS,
            OmniboxFocusReason.FAKE_BOX_TAP, OmniboxFocusReason.FAKE_BOX_LONG_PRESS,
            OmniboxFocusReason.ACCELERATOR_TAP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OmniboxFocusReason {
        int OMNIBOX_TAP = 0;
        int OMNIBOX_LONG_PRESS = 1;
        int FAKE_BOX_TAP = 2;
        int FAKE_BOX_LONG_PRESS = 3;
        int ACCELERATOR_TAP = 4;
        int NUM_ENTRIES = 5;
    }
    private static final EnumeratedHistogramSample ENUMERATED_FOCUS_REASON =
            new EnumeratedHistogramSample(
                    "Android.OmniboxFocusReason", OmniboxFocusReason.NUM_ENTRIES);
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
    private static final int MINIMUM_LOAD_PROGRESS = 5;

    private final AsyncViewProvider<ToolbarLayout> mToolbarProvider;
    @Nullable
    private ToolbarLayout mToolbar;
    private final ToolbarControlContainer mControlContainer;

    private BottomToolbarCoordinator mBottomToolbarCoordinator;
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabModelObserver mTabModelObserver;
    private MenuDelegatePhone mMenuDelegatePhone;
    private final ToolbarModel mToolbarModel;
    private Profile mCurrentProfile;
    private BookmarkBridge mBookmarkBridge;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    @Nullable
    private LocationBar mLocationBar;
    private FindToolbarManager mFindToolbarManager;
    private AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private OverviewModeBehavior mOverviewModeBehavior;
    private LayoutManager mLayoutManager;

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

    private BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private int mFullscreenFocusToken = FullscreenManager.INVALID_TOKEN;
    private int mFullscreenFindInPageToken = FullscreenManager.INVALID_TOKEN;
    private int mFullscreenMenuToken = FullscreenManager.INVALID_TOKEN;
    private int mFullscreenHighlightToken = FullscreenManager.INVALID_TOKEN;

    private int mPreselectedTabId = Tab.INVALID_TAB_ID;

    private boolean mNativeLibraryReady;
    private boolean mTabRestoreCompleted;

    private AppMenuButtonHelper mAppMenuButtonHelper;

    private TextBubble mTextBubble;

    private HomepageStateListener mHomepageStateListener = new HomepageStateListener() {
        @Override
        public void onHomepageStateUpdated() {
            mToolbarProvider.whenLoaded(
                    (toolbar) -> toolbar.onHomeButtonUpdate(HomepageManager.isHomepageEnabled()
                            || FeatureUtilities.isNewTabPageButtonEnabled()));
        }
    };

    private boolean mInitializedWithNative;

    private boolean mShouldUpdateToolbarPrimaryColor = true;
    private int mCurrentThemeColor;

    private OmniboxStartupMetrics mOmniboxStartupMetrics;

    /**
     * Creates a ToolbarManager object.
     *
     * @param controlContainer The container of the toolbar.
     * @param menuHandler The handler for interacting with the menu.
     * @param appMenuPropertiesDelegate Delegate for interactions with the app level menu.
     * @param invalidator Handler for synchronizing invalidations across UI elements.
     * @param urlFocusChangedCallback The callback to be notified when the URL focus changes.
     */
    public ToolbarManager(ChromeActivity activity, ToolbarControlContainer controlContainer,
            final AppMenuHandler menuHandler, AppMenuPropertiesDelegate appMenuPropertiesDelegate,
            Invalidator invalidator, Callback<Boolean> urlFocusChangedCallback) {
        mActivity = activity;
        mActionBarDelegate = new ViewShiftingActionBarDelegate(activity, controlContainer);

        mToolbarModel = new ToolbarModel(activity);
        mControlContainer = controlContainer;
        assert mControlContainer != null;
        mUrlFocusChangedCallback = urlFocusChangedCallback;

        mToolbarActionModeCallback = new ToolbarActionModeCallback();

        mToolbarProvider = AsyncViewProvider.of(controlContainer, R.id.toolbar_stub, R.id.toolbar);
        mToolbarProvider.whenLoaded((toolbar)
                                            -> onToolbarInflationComplete(toolbar, menuHandler,
                                                    appMenuPropertiesDelegate, invalidator));
    }

    private void onToolbarInflationComplete(ToolbarLayout toolbar, final AppMenuHandler menuHandler,
            AppMenuPropertiesDelegate appMenuPropertiesDelegate, Invalidator invalidator) {
        mToolbar = toolbar;
        mActionModeController = new ActionModeController(mActivity, mActionBarDelegate);
        mActionModeController.setCustomSelectionActionModeCallback(mToolbarActionModeCallback);

        MenuDelegatePhone menuDelegate = new MenuDelegatePhone() {
            @Override
            public void updateReloadButtonState(boolean isLoading) {
                if (mAppMenuPropertiesDelegate != null) {
                    mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
                    menuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
                }
            }
        };
        setMenuDelegatePhone(menuDelegate);

        toolbar.setPaintInvalidator(invalidator);
        mActionModeController.setTabStripHeight(toolbar.getTabStripHeight());
        mLocationBar = mToolbar.getLocationBar();
        mLocationBar.setToolbarDataProvider(mToolbarModel);
        mLocationBar.addUrlFocusChangeListener(this);
        mLocationBar.setDefaultTextEditActionModeCallback(
                mActionModeController.getActionModeCallback());
        mLocationBar.initializeControls(
                new WindowDelegate(mActivity.getWindow()), mActivity.getWindowAndroid());

        setMenuHandler(menuHandler);
        toolbar.initialize(mToolbarModel, this, mAppMenuButtonHelper);

        mAppMenuPropertiesDelegate = appMenuPropertiesDelegate;

        HomepageManager.getInstance().addListener(mHomepageStateListener);

        mOmniboxStartupMetrics = new OmniboxStartupMetrics(mActivity);

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                refreshSelectedTab();
                updateTabCount();
                if (mBottomToolbarCoordinator != null) {
                    mBottomToolbarCoordinator.setIncognito(newModel.isIncognito());
                }
            }

            @Override
            public void onTabStateInitialized() {
                mTabRestoreCompleted = true;
                handleTabRestoreCompleted();
            }
        };

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateTabCount();
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                mPreselectedTabId = Tab.INVALID_TAB_ID;
                refreshSelectedTab();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                mLocationBar.setTitleToPageTitle();
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void allTabsPendingClosure(List<Tab> tabs) {
                updateTabCount();
                refreshSelectedTab();
            }

            @Override
            public void tabRemoved(Tab tab) {
                updateTabCount();
                refreshSelectedTab();
            }
        };

        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                if (mToolbarModel.getTab() == null) return;

                assert tab == mToolbarModel.getTab();
                QueryInOmnibox.setIgnoreSecurityLevelForSearchTerms(tab.getProfile(), false);
                mLocationBar.updateSecurityIcon();
                mLocationBar.setUrlToPageUrl();
            }

            @Override
            public void didReloadLoFiImages(Tab tab) {
                mLocationBar.updateSecurityIcon();
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
            public void onPageLoadStarted(Tab tab, String url) {
                QueryInOmnibox.setIgnoreSecurityLevelForSearchTerms(tab.getProfile(), true);
            }

            @Override
            public void onPageLoadFinished(Tab tab) {
                QueryInOmnibox.setIgnoreSecurityLevelForSearchTerms(tab.getProfile(), false);
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
                    mLocationBar.updateSecurityIcon();
                    PreviewsUma.recordLitePageAtLoadFinish(
                            PreviewsAndroidBridge.getInstance().getPreviewsType(
                                    tab.getWebContents()));
                }

                handleIPHForSuccessfulPageLoad(tab);
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                QueryInOmnibox.setIgnoreSecurityLevelForSearchTerms(tab.getProfile(), true);
                updateButtonStatus();
                updateTabLoadingState(true);
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;
                QueryInOmnibox.setIgnoreSecurityLevelForSearchTerms(tab.getProfile(), false);
                updateTabLoadingState(true);

                // If we made some progress, fast-forward to complete, otherwise just dismiss any
                // MINIMUM_LOAD_PROGRESS that had been set.
                if (tab.getProgress() > MINIMUM_LOAD_PROGRESS && tab.getProgress() < 100) {
                    updateLoadProgress(100);
                }
                finishLoadProgress(true);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, int progress) {
                if (NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) return;

                // TODO(kkimlabs): Investigate using float progress all the way up to Blink.
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
                NewTabPage ntp = mToolbarModel.getNewTabPageForCurrentTab();
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
            public void onDidStartNavigation(Tab tab, String url, boolean isInMainFrame,
                    boolean isSameDocument, boolean isErrorPage) {
                if (!isInMainFrame) return;
                // Update URL as soon as it becomes available when it's a new tab.
                // But we want to update only when it's a new tab. So we check whether the current
                // navigation entry is initial, meaning whether it has the same target URL as the
                // initial URL of the tab.
                if (tab.getWebContents() != null
                        && tab.getWebContents().getNavigationController() != null
                        && tab.getWebContents().getNavigationController().isInitialNavigation()) {
                    mLocationBar.setUrlToPageUrl();
                }

                if (isSameDocument) return;
                // This event is used as the primary trigger for the progress bar because it
                // is the earliest indication that a load has started for a particular frame. In
                // the case of the progress bar, it should only traverse the screen a single time
                // per page load. So if this event states the main frame has started loading the
                // progress bar is started.

                if (NativePageFactory.isNativePageUrl(url, tab.isIncognito())) {
                    finishLoadProgress(false);
                    return;
                }

                mLoadProgressSimulator.cancel();
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }

            @Override
            public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                    boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
                    boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                    int httpStatusCode) {
                if (hasCommitted && isInMainFrame && !isSameDocument) {
                    mToolbar.onNavigatedToDifferentPage();
                }

                if (hasCommitted && tab.isPreview()) {
                    // Some previews are not fully decided until the page commits. If this
                    // is a preview, update the security icon which will also update the verbose
                    // status view to make sure the "Lite" badge is displayed.
                    mLocationBar.updateSecurityIcon();
                    PreviewsUma.recordLitePageAtCommit(
                            PreviewsAndroidBridge.getInstance().getPreviewsType(
                                    tab.getWebContents()),
                            isInMainFrame);
                }

                // If the load failed due to a different navigation, there is no need to reset the
                // location bar animations.
                if (errorCode != 0 && isInMainFrame && !hasPendingNonNtpNavigation(tab)) {
                    NewTabPage ntp = mToolbarModel.getNewTabPageForCurrentTab();
                    if (ntp == null) return;

                    ntp.setUrlFocusAnimationsDisabled(false);
                    mToolbar.onTabOrModelChanged();
                    if (mToolbar.getProgressBar() != null) mToolbar.getProgressBar().finish(false);
                }
            }

            @Override
            public void onNavigationEntriesDeleted(Tab tab) {
                if (tab == mToolbarModel.getTab()) {
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
                mToolbar.handleFindToolbarStateChange(true);
                if (mControlsVisibilityDelegate != null) {
                    mFullscreenFindInPageToken =
                            mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                                    mFullscreenFindInPageToken);
                }
            }

            @Override
            public void onFindToolbarHidden() {
                mToolbar.handleFindToolbarStateChange(false);
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

                // For top toolbar we depend on animations instead.
                if (mBottomToolbarCoordinator == null) return;
                MenuButton menuButton = mBottomToolbarCoordinator.getMenuButtonWrapper();
                if (menuButton == null) return;
                menuButton.setUpdateBadgeVisibilityIfValidState(false);
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mToolbar.setTabSwitcherMode(false, showToolbar, delayAnimation);
                updateButtonStatus();

                // For top toolbar we depend on animations instead.
                if (mBottomToolbarCoordinator == null) return;
                MenuButton menuButton = mBottomToolbarCoordinator.getMenuButtonWrapper();
                if (menuButton == null) return;
                menuButton.setUpdateBadgeVisibilityIfValidState(true);
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
    }

    /**
     * @return  Whether the UrlBar currently has focus.
     */
    public boolean isUrlBarFocused() {
        return getToolbarLayout().getLocationBar().isUrlBarFocused();
    }

    /**
     * @param reason A {@link OmniboxFocusReason} that the omnibox was focused.
     */
    public static void recordOmniboxFocusReason(@OmniboxFocusReason int reason) {
        ENUMERATED_FOCUS_REASON.record(reason);
    }

    /**
     * Enable the bottom toolbar.
     */
    public void enableBottomToolbar() {
        if (FeatureUtilities.isBottomToolbarEnabled()) {
            final ToolbarButtonSlotData firstButtonSlot =
                    new ToolbarButtonSlotData(createHomeButton());
            final ToolbarButtonSlotData secondButtonSlot =
                    new ToolbarButtonSlotData(createSearchAccelerator());
            mBottomToolbarCoordinator = new BottomToolbarCoordinator(
                    mActivity.getFullscreenManager(), mActivity.findViewById(R.id.coordinator),
                    firstButtonSlot, secondButtonSlot, mActivity.getActivityTabProvider());
            if (mAppMenuButtonHelper != null) mAppMenuButtonHelper.setMenuShowsFromBottom(true);
        }
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

    private ToolbarButtonData createHomeButton() {
        final OnClickListener homeButtonListener = v -> {
            recordBottomToolbarUseForIPH();
            openHomepage();
        };
        final int homeButtonIcon = FeatureUtilities.isNewTabPageButtonEnabled()
                ? R.drawable.ic_home
                : R.drawable.btn_toolbar_home;
        final Drawable drawable = ContextCompat.getDrawable(mActivity, homeButtonIcon);
        final CharSequence accessibilityString =
                mActivity.getString(R.string.accessibility_toolbar_btn_home);
        return new ToolbarButtonData(
                drawable, accessibilityString, accessibilityString, homeButtonListener, mActivity);
    }

    private ToolbarButtonData createNewTabButton(OnClickListener newTabClickListener) {
        final CharSequence normalAccessibilityString =
                mActivity.getString(R.string.accessibility_toolbar_btn_new_tab);
        final CharSequence incognitoAccessibilityString =
                mActivity.getString(R.string.accessibility_toolbar_btn_new_incognito_tab);

        final Drawable drawable = VectorDrawableCompat.create(
                mActivity.getResources(), R.drawable.new_tab_icon, mActivity.getTheme());
        return new ToolbarButtonData(drawable, normalAccessibilityString,
                incognitoAccessibilityString, newTabClickListener, mActivity);
    }

    private ToolbarButtonData createSearchAccelerator() {
        final OnClickListener searchAcceleratorListener = v -> {
            recordBottomToolbarUseForIPH();
            recordOmniboxFocusReason(OmniboxFocusReason.ACCELERATOR_TAP);
            ACCELERATOR_BUTTON_TAP_ACTION.record();
            setUrlBarFocus(true);
        };
        final Drawable drawable =
                ContextCompat.getDrawable(mActivity, R.drawable.ic_search).mutate();
        final CharSequence accessibilityString =
                mActivity.getString(R.string.accessibility_toolbar_btn_search_accelerator);
        return new ToolbarButtonData(drawable, accessibilityString, accessibilityString,
                searchAcceleratorListener, mActivity);
    }

    private ToolbarButtonData createIncognitoToggleButton(
            OnClickListener incognitoToggleClickHandler) {
        final CharSequence normalAccessibilityString =
                mActivity.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard);
        final CharSequence incognitoAccessibilityString =
                mActivity.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_incognito);
        final Drawable drawable = ContextCompat.getDrawable(mActivity, R.drawable.incognito_simple);
        return new ToolbarButtonData(drawable, normalAccessibilityString,
                incognitoAccessibilityString, incognitoToggleClickHandler, mActivity);
    }

    /**
     * @return Whether the bottom toolbar is currently enabled (an activity may or may not enable
     *         this feature).
     */
    public boolean isBottomToolbarEnabled() {
        return mBottomToolbarCoordinator != null;
    }

    /**
     * @return The coordinator for the bottom toolbar if it exists.
     */
    public BottomToolbarCoordinator getBottomToolbarCoordinator() {
        return mBottomToolbarCoordinator;
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     * @param tab The current tab.
     * @param featureName The associated feature name.
     */
    public void showDownloadPageTextBubble(final Tab tab, String featureName) {
        if (tab == null || mToolbar == null) return;

        // TODO(shaktisahu): Find out if the download menu button is enabled (crbug/712438).
        ChromeActivity activity = tab.getActivity();
        if (!(activity instanceof ChromeTabbedActivity) || activity.isTablet()
                || activity.isInOverviewMode() || !DownloadUtils.isAllowedToDownloadPage(tab)) {
            return;
        }

        final Tracker tracker = TrackerFactory.getTrackerForProfile(tab.getProfile());

        if (!tracker.shouldTriggerHelpUI(featureName)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(getMenuButton());
        int yInsetPx = mToolbar.getContext().getResources().getDimensionPixelOffset(
                R.dimen.text_bubble_menu_anchor_y_inset);
        rectProvider.setInsetPx(0, 0, 0, yInsetPx);
        mTextBubble = new TextBubble(mToolbar.getContext(), getMenuButton(),
                R.string.iph_download_page_for_offline_usage_text,
                R.string.iph_download_page_for_offline_usage_accessibility_text, rectProvider);
        mTextBubble.setDismissOnTouchInteraction(true);
        mTextBubble.addOnDismissListener(() -> {
            mHandler.postDelayed(() -> {
                tracker.dismissed(featureName);
                activity.getAppMenuHandler().setMenuHighlight(null);
            }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS);
        });
        activity.getAppMenuHandler().setMenuHighlight(R.id.offline_page_id);
        mTextBubble.show();

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
     * Initialize the manager with the components that had native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector           The selector that handles tab management.
     * @param controlsVisibilityDelegate The delegate to handle visibility of browser controls.
     * @param findToolbarManager         The manager for find in page.
     * @param overviewModeBehavior       The overview mode manager.
     * @param layoutManager              A {@link LayoutManager} instance used to watch for scene
     *                                   changes.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            FindToolbarManager findToolbarManager, OverviewModeBehavior overviewModeBehavior,
            LayoutManager layoutManager, OnClickListener tabSwitcherClickHandler,
            OnClickListener newTabClickHandler, OnClickListener bookmarkClickHandler,
            OnClickListener customTabsBackClickHandler, OnClickListener incognitoClickHandler) {
        assert !mInitializedWithNative;

        mToolbarProvider.whenLoaded((toolbar) -> {
            mTabModelSelector = tabModelSelector;
            toolbar.setTabModelSelector(mTabModelSelector);
            toolbar.getLocationBar().updateVisualsForState();
            toolbar.getLocationBar().setUrlToPageUrl();
            toolbar.setBrowserControlsVisibilityDelegate(controlsVisibilityDelegate);
            toolbar.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
            toolbar.setOnNewTabClickHandler(newTabClickHandler);
            toolbar.setBookmarkClickHandler(bookmarkClickHandler);
            toolbar.setCustomTabCloseClickHandler(customTabsBackClickHandler);
            toolbar.setIncognitoClickHandler(incognitoClickHandler);
            toolbar.setLayoutUpdateHost(layoutManager);

            toolbar.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
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

            mToolbarModel.initializeWithNative();

            mFindToolbarManager = findToolbarManager;

            assert controlsVisibilityDelegate != null;
            mControlsVisibilityDelegate = controlsVisibilityDelegate;

            mNativeLibraryReady = false;

            mFindToolbarManager.addObserver(mFindToolbarObserver);

            if (overviewModeBehavior != null) {
                mOverviewModeBehavior = overviewModeBehavior;
                mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
            }
            if (layoutManager != null) {
                mLayoutManager = layoutManager;
                mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);
            }

            if (mBottomToolbarCoordinator != null) {
                final boolean showIncognitoToggleButton =
                        !DeviceClassManager.enableAccessibilityLayout()
                        && ChromeFeatureList.isEnabled(
                                   ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                        && PrefServiceBridge.getInstance().isIncognitoModeEnabled();
                final ToolbarButtonData firstSlotTabSwitcherButtonData = showIncognitoToggleButton
                        ? createIncognitoToggleButton(
                                  wrapBottomToolbarClickListenerForIPH(incognitoClickHandler))
                        : null;
                mAppMenuButtonHelper.setOnClickRunnable(() -> recordBottomToolbarUseForIPH());
                mBottomToolbarCoordinator.initializeWithNative(
                        mActivity.getCompositorViewHolder().getResourceManager(),
                        mActivity.getCompositorViewHolder().getLayoutManager(),
                        wrapBottomToolbarClickListenerForIPH(tabSwitcherClickHandler),
                        mAppMenuButtonHelper, mTabModelSelector, mOverviewModeBehavior,
                        mActivity.getWindowAndroid(), firstSlotTabSwitcherButtonData,
                        createNewTabButton(
                                wrapBottomToolbarClickListenerForIPH(newTabClickHandler)),
                        tabModelSelector.getCurrentModel().isIncognito());

                // Allow the bottom toolbar to be focused in accessibility after the top toolbar.
                ApiCompatibilityUtils.setAccessibilityTraversalBefore(
                        mLocationBar.getContainerView(), R.id.bottom_toolbar);
            }

            onNativeLibraryReady();
            mInitializedWithNative = true;
        });
    }

    /**
     * Show the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void showAppMenuUpdateBadge() {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.showAppMenuUpdateBadge());
        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.showAppMenuUpdateBadge();
        }
    }

    /**
     * Remove the update badge in both the top and bottom toolbar.
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public void removeAppMenuUpdateBadge(boolean animate) {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.removeAppMenuUpdateBadge(animate));
        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.removeAppMenuUpdateBadge();
        }
    }

    /**
     * @return Whether the badge is showing (either in the top or bottom toolbar).
     * TODO(amaralp): Only the top or bottom menu should be visible.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        if (mBottomToolbarCoordinator != null
                && mBottomToolbarCoordinator.isShowingAppMenuUpdateBadge()) {
            return true;
        }
        if (mToolbar == null) return false;
        return mToolbar.isShowingAppMenuUpdateBadge();
    }

    /**
     * Enable the experimental toolbar button.
     * @param onClickListener The {@link OnClickListener} to be called when the button is clicked.
     * @param drawableResId The resource id of the drawable to display for the button.
     * @param contentDescriptionResId The resource id of the content description for the button.
     */
    public void enableExperimentalButton(OnClickListener onClickListener,
            @DrawableRes int drawableResId, @StringRes int contentDescriptionResId) {
        mToolbarProvider.whenLoaded((toolbar) -> {
            toolbar.enableExperimentalButton(
                    onClickListener, drawableResId, contentDescriptionResId);
        });
    }

    /**
     * Disable the experimental toolbar button.
     */
    public void disableExperimentalButton() {
        mToolbarProvider.whenLoaded((t) -> t.disableExperimentalButton());
    }

    /**
     * @return The experimental toolbar button if it exists.
     */
    public @Nullable View getExperimentalButtonView() {
        if (mToolbar == null) return null;
        return mToolbar.getExperimentalButtonView();
    }

    /**
     * @return The bookmarks bridge.
     */
    public BookmarkBridge getBookmarkBridge() {
        return mBookmarkBridge;
    }

    /** Return the toolbar model for testing purposes. */
    public ToolbarModel getToolbarModelForTesting() {
        return mToolbarModel;
    }

    /**
     * @return The toolbar interface that this manager handles.
     */
    @Nullable
    public Toolbar getToolbar() {
        return mToolbar;
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    @VisibleForTesting
    public ToolbarLayout getToolbarLayout() {
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

    /**
     * @return The view containing the pop up menu button.
     */
    public View getMenuButton() {
        if (mBottomToolbarCoordinator != null) {
            return mBottomToolbarCoordinator.getMenuButtonWrapper().getImageButton();
        }
        if (mToolbar != null) return mToolbar.getMenuButton();
        return null;
    }

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    public View getSecurityIconView() {
        if (mToolbar == null || mToolbar.getLocationBar() == null) return null;
        return mToolbar.getLocationBar().getSecurityIconView();
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
        mToolbarProvider.whenLoaded(
                (t) -> t.addCustomActionButton(drawable, description, listener));
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
        mToolbarProvider.whenLoaded(
                (t) -> t.updateCustomActionButton(index, drawable, description));
    }

    /**
     * Call to tear down all of the toolbar dependencies.
     */
    public void destroy() {
        if (mInitializedWithNative) {
            HomepageManager.getInstance().removeListener(mHomepageStateListener);
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
        }
        if (mTemplateUrlObserver != null) {
            TemplateUrlService.getInstance().removeObserver(mTemplateUrlObserver);
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

        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.destroy();
            mBottomToolbarCoordinator = null;
        }

        if (mOmniboxStartupMetrics != null) {
            // Record the histogram before destroying, if we have the data.
            mOmniboxStartupMetrics.maybeRecordHistograms();
            mOmniboxStartupMetrics.destroy();
            mOmniboxStartupMetrics = null;
        }

        if (mToolbarProvider != null) {
            mToolbarProvider.destroy((toolbar) -> {
                toolbar.getLocationBar().removeUrlFocusChangeListener(this);
                toolbar.destroy();
            });
        }

        if (mTabObserver != null) {
            Tab currentTab = mToolbarModel.getTab();
            if (currentTab != null) currentTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }
        mToolbarModel.destroy();
        mHandler.removeCallbacksAndMessages(null); // Cancel delayed tasks.
    }

    /**
     * Called when the orientation of the activity has changed.
     */
    public void onOrientationChange() {
        if (mActionModeController == null) return;
        mActionModeController.showControlsOnOrientationChange();
    }

    /**
     * Called when the accessibility enabled state changes.
     * @param enabled Whether accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.onAccessibilityStatusChanged(enabled));
    }

    private void registerTemplateUrlObserver() {
        final TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
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
                mToolbar.onDefaultSearchEngineChanged();
            }
        };
        templateUrlService.addObserver(mTemplateUrlObserver);
    }

    private void onNativeLibraryReady() {
        mNativeLibraryReady = true;
        mToolbar.onNativeLibraryReady();

        final TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
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
    }

    private void handleTabRestoreCompleted() {
        if (!mTabRestoreCompleted || !mNativeLibraryReady) return;
        mToolbar.onStateRestored();
        updateTabCount();
    }

    /**
     * Sets the handler for any special case handling related with the menu button.
     * @param menuHandler The handler to be used.
     */
    private void setMenuHandler(AppMenuHandler menuHandler) {
        menuHandler.addObserver(new AppMenuObserver() {
            @Override
            public void onMenuVisibilityChanged(boolean isVisible) {
                if (isVisible) {
                    // Defocus here to avoid handling focus in multiple places, e.g., when the
                    // forward button is pressed. (see crbug.com/414219)
                    setUrlBarFocus(false);
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
                mToolbar.setMenuButtonHighlight(highlighting);

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
        mAppMenuButtonHelper = new AppMenuButtonHelper(menuHandler);
        mAppMenuButtonHelper.setOnAppMenuShownListener(() -> {
            RecordUserAction.record("MobileToolbarShowMenu");
            mToolbar.onMenuShown();

            // Assume data saver footer is shown only if data reduction proxy is enabled and
            // Chrome home is not
            if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
                tracker.notifyEvent(EventConstants.OVERFLOW_OPENED_WITH_DATA_SAVER_SHOWN);
            }
        });
    }

    @Nullable
    private MenuButton getMenuButtonWrapper() {
        if (mBottomToolbarCoordinator != null) {
            return mBottomToolbarCoordinator.getMenuButtonWrapper();
        }

        if (mToolbar == null) return null;
        View menuButtonWrapper = mToolbar.getMenuButtonWrapper();
        if (menuButtonWrapper instanceof MenuButton) return (MenuButton) menuButtonWrapper;
        return null;
    }

    /**
     * Set the delegate that will handle updates from toolbar driven state changes.
     * @param menuDelegatePhone The menu delegate to be updated (only applicable to phones).
     */
    private void setMenuDelegatePhone(MenuDelegatePhone menuDelegatePhone) {
        mMenuDelegatePhone = menuDelegatePhone;
    }

    @Override
    public boolean back() {
        Tab tab = mToolbarModel.getTab();
        if (tab != null && tab.canGoBack()) {
            tab.goBack();
            updateButtonStatus();
            return true;
        }
        return false;
    }

    @Override
    public boolean forward() {
        Tab tab = mToolbarModel.getTab();
        if (tab != null && tab.canGoForward()) {
            tab.goForward();
            updateButtonStatus();
            return true;
        }
        return false;
    }

    @Override
    public void stopOrReloadCurrentTab() {
        Tab currentTab = mToolbarModel.getTab();
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

        Tab currentTab = mToolbarModel.getTab();
        if (currentTab == null) return;
        String homePageUrl = HomepageManager.getHomepageUri();
        boolean isNewTabPageButtonEnabled = FeatureUtilities.isNewTabPageButtonEnabled();
        if (TextUtils.isEmpty(homePageUrl) || isNewTabPageButtonEnabled) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        if (isNewTabPageButtonEnabled) {
            recordToolbarUseForIPH(EventConstants.CLEAR_TAB_BUTTON_CLICKED);
        } else {
            recordToolbarUseForIPH(EventConstants.HOMEPAGE_BUTTON_CLICKED);
        }
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    @Override
    public void openMemexUI() {
        TabModel model = mTabModelSelector.getModel(false);
        for (int i = 0; i < model.getCount(); i++) {
            String url = model.getTabAt(i).getUrl();
            if (url.startsWith(UrlConstants.CHROME_MEMEX_URL)
                    || url.startsWith(UrlConstants.CHROME_MEMEX_DEV_URL)) {
                model.setIndex(i, TabSelectionType.FROM_USER);
                return;
            }
        }

        mTabModelSelector.openNewTab(
                new LoadUrlParams(UrlConstants.CHROME_MEMEX_URL, PageTransition.AUTO_BOOKMARK),
                TabLaunchType.FROM_EXTERNAL_APP, null, false);
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        assert mToolbar != null;

        mToolbar.onUrlFocusChange(hasFocus);

        if (hasFocus) mOmniboxStartupMetrics.onUrlBarFocused();

        if (mFindToolbarManager != null && hasFocus) mFindToolbarManager.hideToolbar();

        if (mControlsVisibilityDelegate == null) return;
        if (hasFocus) {
            mFullscreenFocusToken = mControlsVisibilityDelegate
                    .showControlsPersistentAndClearOldToken(mFullscreenFocusToken);
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
    public void updatePrimaryColor(int color, boolean shouldAnimate) {
        if (!mShouldUpdateToolbarPrimaryColor) return;

        boolean colorChanged = mCurrentThemeColor != color;
        if (!colorChanged) return;

        mCurrentThemeColor = color;
        mToolbarModel.setPrimaryColor(color);
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.onPrimaryColorChanged(shouldAnimate));
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
        return mToolbarModel.getPrimaryColor();
    }

    /**
     * Prevents the shadow from being rendered.
     */
    public void disableShadow() {
        View toolbarShadow = mControlContainer.findViewById(R.id.toolbar_shadow);
        if (toolbarShadow != null) UiUtils.removeViewFromParent(toolbarShadow);
    }

    /**
     * Sets the drawable that the close button shows, or hides it if {@code drawable} is
     * {@code null}.
     */
    public void setCloseButtonDrawable(Drawable drawable) {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.setCloseButtonImageResource(drawable));
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.getLocationBar().setShowTitle(showTitle));
    }

    /**
     * @see ToolbarLayout#setUrlBarHidden(boolean)
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbarProvider.whenLoaded((toolbar) -> toolbar.setUrlBarHidden(hidden));
    }

    /**
     * @see ToolbarLayout#getContentPublisher()
     */
    public String getContentPublisher() {
        if (mToolbar != null) return mToolbar.getContentPublisher();
        return null;
    }

    /**
     * Focuses or unfocuses the URL bar.
     *
     * If you request focus and the UrlBar was already focused, this will select all of the text.
     *
     * @param focused Whether URL bar should be focused.
     */
    public void setUrlBarFocus(boolean focused) {
        if (!isInitialized()) return;
        boolean wasFocused = mLocationBar.isUrlBarFocused();
        mLocationBar.setUrlBarFocus(focused);
        if (wasFocused && focused) {
            mLocationBar.selectAll();
        }
    }

    /**
     * Reverts any pending edits of the location bar and reset to the page state.  This does not
     * change the focus state of the location bar.
     */
    public void revertLocationBarChanges() {
        if (mToolbar == null) return;
        mLocationBar.revertChanges();
    }

    /**
     * Handle all necessary tasks that can be delayed until initialization completes.
     * @param activityCreationTimeMs The time of creation for the activity this toolbar belongs to.
     * @param activityName Simple class name for the activity this toolbar belongs to.
     */
    public void onDeferredStartup(final long activityCreationTimeMs,
            final String activityName) {
        // Record startup performance statistics
        long elapsedTime = SystemClock.elapsedRealtime() - activityCreationTimeMs;
        if (elapsedTime < RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS) {
            ThreadUtils.postOnUiThreadDelayed(() -> {
                onDeferredStartup(activityCreationTimeMs, activityName);
            }, RECORD_UMA_PERFORMANCE_METRICS_DELAY_MS - elapsedTime);
            return;
        }
        RecordHistogram.recordTimesHistogram("MobileStartup.ToolbarFirstDrawTime2." + activityName,
                mToolbar.getFirstDrawTime() - activityCreationTimeMs, TimeUnit.MILLISECONDS);

        // mOmniboxStartupMetrics might be null. ie. ToolbarManager is destroyed. See
        // https://crbug.com/860449
        if (mOmniboxStartupMetrics != null) mOmniboxStartupMetrics.maybeRecordHistograms();
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        if (isInitialized()) mToolbar.finishAnimations();
    }

    /**
     * Updates the current number of Tabs based on the TabModel this Toolbar contains.
     */
    private void updateTabCount() {
        assert mToolbar != null;
        if (!mTabRestoreCompleted) return;
        final int numberOfTabs = mTabModelSelector.getCurrentModel().getCount();
        mToolbar.updateTabCountVisuals(numberOfTabs);
    }

    /**
     * Updates the current button states and calls appropriate abstract visibility methods, giving
     * inheriting classes the chance to update the button visuals as well.
     */
    private void updateButtonStatus() {
        assert mToolbar != null;
        Tab currentTab = mToolbarModel.getTab();
        boolean tabCrashed = currentTab != null && SadTab.isShowing(currentTab);

        mToolbar.updateButtonVisibility();
        mToolbar.updateBackButtonVisibility(currentTab != null && currentTab.canGoBack());
        mToolbar.updateForwardButtonVisibility(currentTab != null && currentTab.canGoForward());
        updateReloadState(tabCrashed);
        updateBookmarkButtonStatus();
        if (mToolbar.getMenuButtonWrapper() != null) {
            mToolbar.getMenuButtonWrapper().setVisibility(View.VISIBLE);
        }
    }

    private void updateBookmarkButtonStatus() {
        assert mToolbar != null;
        Tab currentTab = mToolbarModel.getTab();
        boolean isBookmarked = currentTab != null
                && currentTab.getBookmarkId() != Tab.INVALID_BOOKMARK_ID;
        boolean editingAllowed = currentTab == null || mBookmarkBridge == null
                || mBookmarkBridge.isEditBookmarksEnabled();
        mToolbar.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    private void updateReloadState(boolean tabCrashed) {
        assert mToolbar != null;
        Tab currentTab = mToolbarModel.getTab();
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
        assert mToolbar != null;
        Tab tab = null;
        if (mPreselectedTabId != Tab.INVALID_TAB_ID) {
            tab = mTabModelSelector.getTabById(mPreselectedTabId);
        }
        if (tab == null) tab = mTabModelSelector.getCurrentTab();

        boolean wasIncognito = mToolbarModel.isIncognito();
        Tab previousTab = mToolbarModel.getTab();

        boolean isIncognito =
                tab != null ? tab.isIncognito() : mTabModelSelector.isIncognitoSelected();
        mToolbarModel.setTab(tab, isIncognito);

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
                    previousTab.setIsAllowedToReturnToExternalApp(false);
                }
                if (tab != null) tab.addObserver(mTabObserver);
            }
            int defaultPrimaryColor =
                    ColorUtils.getDefaultThemeColor(mToolbar.getResources(), isIncognito);
            int primaryColor = tab != null ? tab.getThemeColor() : defaultPrimaryColor;
            updatePrimaryColor(primaryColor, false);

            mToolbar.onTabOrModelChanged();

            if (tab != null && tab.getWebContents() != null
                    && tab.getWebContents().isLoadingToDifferentDocument()) {
                mToolbar.onNavigatedToDifferentPage();
            }

            // Ensure the URL bar loses focus if the tab it was interacting with is changed from
            // underneath it.
            setUrlBarFocus(false);

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
                mAppMenuPropertiesDelegate.setBookmarkBridge(mBookmarkBridge);
                mLocationBar.setAutocompleteProfile(profile);
            }
            mCurrentProfile = profile;
        }

        updateButtonStatus();
    }

    private void updateCurrentTabDisplayStatus() {
        assert mToolbar != null;

        Tab tab = mToolbarModel.getTab();
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
        assert mToolbar != null;
        mLocationBar.updateLoadingState(updateUrl);
        if (updateUrl) updateButtonStatus();
    }

    private void updateLoadProgress(int progress) {
        assert mToolbar != null;
        // If it's a native page, progress bar is already hidden or being hidden, so don't update
        // the value.
        // TODO(kkimlabs): Investigate back/forward navigation with native page & web content and
        //                 figure out the correct progress bar presentation.
        Tab tab = mToolbarModel.getTab();
        if (tab == null || NativePageFactory.isNativePageUrl(tab.getUrl(), tab.isIncognito())) {
            return;
        }

        progress = Math.max(progress, MINIMUM_LOAD_PROGRESS);
        mToolbar.setLoadProgress(progress / 100f);
        if (progress == 100) finishLoadProgress(true);
    }

    private void finishLoadProgress(boolean delayed) {
        assert mToolbar != null;
        mLoadProgressSimulator.cancel();
        mToolbar.finishLoadProgress(delayed);
    }

    /**
     * Only start showing the progress bar if it is not already started.
     */
    private void startLoadProgress() {
        assert mToolbar != null;
        if (mToolbar.isProgressStarted()) return;
        mToolbar.startLoadProgress();
    }

    public void setProgressBarEnabled(boolean enabled) {
        mToolbarProvider.whenLoaded((toolbar)
                                            -> toolbar.getProgressBar().setVisibility(
                                                    enabled ? View.VISIBLE : View.GONE));
    }

    private boolean shouldShowCursorInLocationBar() {
        Tab tab = mToolbarModel.getTab();
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

        private static final int PROGRESS_INCREMENT = 10;
        private static final int PROGRESS_INCREMENT_DELAY_MS = 10;

        private final ToolbarManager mToolbarManager;
        private final Handler mHandler;

        private int mProgress;

        public LoadProgressSimulator(ToolbarManager toolbar) {
            mToolbarManager = toolbar;
            mHandler = new Handler(Looper.getMainLooper()) {
                @Override
                public void handleMessage(Message msg) {
                    assert msg.what == MSG_ID_UPDATE_PROGRESS;
                    mProgress = Math.min(100, mProgress += PROGRESS_INCREMENT);
                    mToolbarManager.updateLoadProgress(mProgress);

                    if (mProgress == 100) {
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
}
