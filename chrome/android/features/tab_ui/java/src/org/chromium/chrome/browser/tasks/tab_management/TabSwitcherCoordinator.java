// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabSelectionEditorOpenMetricGroups;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator.SystemUiScrimDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.ArrayList;
import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid or carousel of tabs for the main
 * TabSwitcher UI.
 */
public class TabSwitcherCoordinator
        implements DestroyObserver, TabSwitcher, TabSwitcher.TabListDelegate,
                   TabSwitcherMediator.ResetHandler, TabSwitcherMediator.MessageItemsController,
                   TabSwitcherMediator.PriceWelcomeMessageController,
                   TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener {
    /**
     * Interface to control the IPH dialog.
     */
    interface IphController {
        /**
         * Show the dialog with IPH.
         */
        void showIph();
    }

    // TODO(crbug.com/982018): Rename 'COMPONENT_NAME' so as to add different metrics for carousel
    // tab switcher.
    static final String COMPONENT_NAME = "GridTabSwitcher";
    private static boolean sAppendedMessagesForTesting;
    private final Activity mActivity;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabListCoordinator;
    private final TabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final ScrimCoordinator mGridDialogScrimCoordinator;
    private final boolean mUsesTabGridDialogCoordinator;
    @Nullable
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final TabModelSelector mTabModelSelector;
    private final @TabListCoordinator.TabListMode int mMode;
    private final MessageCardProviderCoordinator mMessageCardProviderCoordinator;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final Supplier<DynamicResourceLoader> mDynamicResourceLoaderSupplier;
    private final SnackbarManager mSnackbarManager;
    private final ModalDialogManager mModalDialogManager;
    @Nullable
    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    @Nullable
    private List<TabSelectionEditorAction> mTabSelectionEditorActions;
    private TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;
    private TabAttributeCache mTabAttributeCache;
    private ViewGroup mContainer;
    private TabCreatorManager mTabCreatorManager;
    private boolean mIsInitialized;
    private PriceMessageService mPriceMessageService;
    private SharedPreferencesManager.Observer mPriceAnnotationsPrefObserver;
    private final ViewGroup mCoordinatorView;
    private final ViewGroup mRootView;
    private TabContentManager mTabContentManager;
    private IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    /**
     * TODO(crbug.com/1227656): Refactor this to pass a supplier instead to ensure we re-use the
     * same instance of {@link IncognitoReauthManager} across the codebase.
     */
    private IncognitoReauthManager mIncognitoReauthManager;

    private final MenuOrKeyboardActionController
            .MenuOrKeyboardActionHandler mTabSwitcherMenuActionHandler;
    private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;
    private TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;
    private SnackbarManager mTabSelectionEditorSnackbarManager;

    /** {@see TabManagementDelegate#createCarouselTabSwitcher} */
    public TabSwitcherCoordinator(@NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControls,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull ViewGroup container,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator scrimCoordinator, @TabListMode int mode,
            @NonNull ViewGroup rootView,
            @NonNull Supplier<DynamicResourceLoader> dynamicResourceLoaderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager) {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherCoordinator.constructor")) {
            mActivity = activity;
            mMode = mode;
            mTabModelSelector = tabModelSelector;
            mContainer = container;
            mCoordinatorView = activity.findViewById(R.id.coordinator);
            mTabCreatorManager = tabCreatorManager;
            mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
            mRootView = rootView;
            mTabContentManager = tabContentManager;
            mDynamicResourceLoaderSupplier = dynamicResourceLoaderSupplier;
            mSnackbarManager = snackbarManager;
            mModalDialogManager = modalDialogManager;

            // The snackbarManager for the TabSelectionEditor from the tab switcher side, with the
            // rootView as the default parentView. The parentView will be re-parented on show,
            // inside the selection editor mediator using its layout.
            mTabSelectionEditorSnackbarManager = new SnackbarManager(activity, mRootView, null);

            PropertyModel containerViewModel =
                    new PropertyModel(TabListContainerProperties.ALL_KEYS);

            OneshotSupplier<TabGridDialogMediator.DialogController> dialogControllerSupplier = null;
            if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(activity)) {
                mGridDialogScrimCoordinator =
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mRootView.getContext())
                        ? createScrimCoordinator()
                        : scrimCoordinator;
                mUsesTabGridDialogCoordinator = true;
                dialogControllerSupplier =
                        new OneshotSupplier<TabGridDialogMediator.DialogController>() {
                            // Implementation is based on OneshotSupplierImpl with modifications
                            // such that onAvailable does not invoke get() unless the object already
                            // exists this prevents callers of onAvailable from triggering the lazy
                            // creation of the TabGridDialogCoordinator before it is required.
                            private final Promise<TabGridDialogMediator.DialogController> mPromise =
                                    new Promise<>();
                            private final ThreadUtils.ThreadChecker mThreadChecker =
                                    new ThreadUtils.ThreadChecker();

                            @Override
                            public TabGridDialogMediator.DialogController onAvailable(
                                    Callback<TabGridDialogMediator.DialogController> callback) {
                                mThreadChecker.assertOnValidThread();
                                mPromise.then(callback);
                                if (!hasValue()) return null;

                                return get();
                            }

                            @Override
                            public TabGridDialogMediator.DialogController get() {
                                mThreadChecker.assertOnValidThread();
                                if (initTabGridDialogCoordinator()) {
                                    assert !mPromise.isFulfilled();
                                    mPromise.fulfill(
                                            mTabGridDialogCoordinator.getDialogController());
                                }
                                assert mPromise.isFulfilled();
                                return mPromise.getResult();
                            }

                            @Override
                            public boolean hasValue() {
                                return mTabGridDialogCoordinator != null;
                            }
                        };
            } else {
                mGridDialogScrimCoordinator = null;
                mUsesTabGridDialogCoordinator = false;
                mTabGridDialogCoordinator = null;
            }
            mMediator = new TabSwitcherMediator(activity, this, containerViewModel,
                    tabModelSelector, browserControls, container, tabContentManager, this, this,
                    multiWindowModeStateDispatcher, mode, incognitoReauthControllerSupplier,
                    backPressManager, dialogControllerSupplier);

            mTabSwitcherCustomViewManager = new TabSwitcherCustomViewManager(mMediator);

            mMultiThumbnailCardProvider =
                    new MultiThumbnailCardProvider(activity, tabContentManager, tabModelSelector);

            PseudoTab.TitleProvider titleProvider = (context, tab) -> {
                int numRelatedTabs =
                        PseudoTab.getRelatedTabs(context, tab, tabModelSelector).size();
                if (numRelatedTabs == 1) return tab.getTitle();

                return TabGroupTitleEditor.getDefaultTitle(context, numRelatedTabs);
            };

            long startTimeMs = SystemClock.uptimeMillis();
            mTabListCoordinator = new TabListCoordinator(mode, activity, tabModelSelector,
                    mMultiThumbnailCardProvider, titleProvider, true, mMediator, null,
                    TabProperties.UiType.CLOSABLE, null, this, container, true, COMPONENT_NAME,
                    mRootView, null);
            mTabListCoordinator.setOnLongPressTabItemEventListener(this);
            mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                    mTabListCoordinator.getContainerView(), TabListContainerViewBinder::bind);

            RecordHistogram.recordTimesHistogram("Android.TabSwitcher.SetupRecyclerView.Time",
                    SystemClock.uptimeMillis() - startTimeMs);

            mMessageCardProviderCoordinator = new MessageCardProviderCoordinator(
                    activity, tabModelSelector::isIncognitoSelected, (identifier) -> {
                        if (identifier == MessageService.MessageType.PRICE_MESSAGE
                                || identifier
                                        == MessageService.MessageType
                                                   .INCOGNITO_REAUTH_PROMO_MESSAGE) {
                            mTabListCoordinator.removeSpecialListItem(
                                    TabProperties.UiType.LARGE_MESSAGE, identifier);
                        } else {
                            mTabListCoordinator.removeSpecialListItem(
                                    TabProperties.UiType.MESSAGE, identifier);
                            appendNextMessage(identifier);
                        }
                    });

            mMenuOrKeyboardActionController = menuOrKeyboardActionController;

            if (mode == TabListCoordinator.TabListMode.GRID) {
                if (shouldRegisterMessageItemType()) {
                    mTabListCoordinator.registerItemType(TabProperties.UiType.MESSAGE,
                            new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                            MessageCardViewBinder::bind);
                }

                if (shouldRegisterLargeMessageItemType()) {
                    mTabListCoordinator.registerItemType(TabProperties.UiType.LARGE_MESSAGE,
                            new LayoutViewBuilder(R.layout.large_message_card_item),
                            LargeMessageCardViewBinder::bind);
                }

                if (PriceTrackingFeatures.isPriceTrackingEnabled()) {
                    mPriceAnnotationsPrefObserver = key -> {
                        if (PriceTrackingUtilities.TRACK_PRICES_ON_TABS.equals(key)
                                && !mTabModelSelector.isIncognitoSelected()
                                && mTabModelSelector.isTabStateInitialized()) {
                            resetWithTabList(mTabModelSelector.getTabModelFilterProvider()
                                                     .getCurrentTabModelFilter(),
                                    false, isShowingTabsInMRUOrder(mMode));
                        }
                    };
                    SharedPreferencesManager.getInstance().addObserver(
                            mPriceAnnotationsPrefObserver);
                }
            }

            if (mode == TabListCoordinator.TabListMode.GRID
                    || mode == TabListCoordinator.TabListMode.LIST) {
                mTabSwitcherMenuActionHandler =
                        new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                            @Override
                            public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                                if (id == R.id.menu_select_tabs) {
                                    showTabSelectionEditor();
                                    RecordUserAction.record("MobileMenuSelectTabs");
                                    return true;
                                }
                                return false;
                            }
                        };
                mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                        mTabSwitcherMenuActionHandler);
            } else {
                mTabSwitcherMenuActionHandler = null;
            }

            if (ChromeFeatureList.sInstantStart.isEnabled()) {
                mTabAttributeCache = new TabAttributeCache(mTabModelSelector);
            }

            mLifecycleDispatcher = lifecycleDispatcher;
            mLifecycleDispatcher.register(this);
        }
    }

    /**
     * @return false if already initialized or true when first initialized.
     */
    private boolean initTabGridDialogCoordinator() {
        assert mUsesTabGridDialogCoordinator;
        if (mTabGridDialogCoordinator != null) return false;

        mTabGridDialogCoordinator =
                new TabGridDialogCoordinator(mActivity, mTabModelSelector, mTabContentManager,
                        mTabCreatorManager, mCoordinatorView, TabSwitcherCoordinator.this,
                        mMediator, TabSwitcherCoordinator.this::getTabGridDialogAnimationSourceView,
                        mGridDialogScrimCoordinator, mTabListCoordinator.getTabGroupTitleEditor(),
                        mRootView);
        return true;
    }

    private ScrimCoordinator createScrimCoordinator() {
        ViewGroup coordinator = mActivity.findViewById(R.id.coordinator);
        SystemUiScrimDelegate delegate = new SystemUiScrimDelegate() {
            @Override
            public void setStatusBarScrimFraction(float scrimFraction) {}

            @Override
            public void setNavigationBarScrimFraction(float scrimFraction) {}
        };
        return new ScrimCoordinator(mActivity, delegate, coordinator,
                coordinator.getContext().getColor(R.color.omnibox_focused_fading_background_color));
    }

    @VisibleForTesting
    public static boolean hasAppendedMessagesForTesting() {
        return sAppendedMessagesForTesting;
    }

    @Override
    public void initWithNative() {
        if (mIsInitialized) return;
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherCoordinator.initWithNative")) {
            mTabListCoordinator.initWithNative(mDynamicResourceLoaderSupplier.get());

            if (mMode == TabListCoordinator.TabListMode.GRID) {
                if (ChromeFeatureList.sCloseTabSuggestions.isEnabled()) {
                    mTabSuggestionsOrchestrator = new TabSuggestionsOrchestrator(
                            mActivity, mTabModelSelector, mLifecycleDispatcher);
                    TabSuggestionMessageService tabSuggestionMessageService =
                            new TabSuggestionMessageService(mActivity, mTabModelSelector, () -> {
                                initTabSelectionEditor();
                                return mTabSelectionEditorCoordinator.getController();
                            });
                    mTabSuggestionsOrchestrator.addObserver(tabSuggestionMessageService);
                    mMessageCardProviderCoordinator.subscribeMessageService(
                            tabSuggestionMessageService);
                }

                if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mActivity)
                        && !TabSwitcherCoordinator.isShowingTabsInMRUOrder(mMode)) {
                    mTabGridIphDialogCoordinator = new TabGridIphDialogCoordinator(
                            mActivity, mContainer, mModalDialogManager);
                    IphMessageService iphMessageService =
                            new IphMessageService(mTabGridIphDialogCoordinator);
                    mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);
                }

                if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()
                        && mIncognitoReauthPromoMessageService == null) {
                    mIncognitoReauthManager = new IncognitoReauthManager();
                    mIncognitoReauthPromoMessageService = new IncognitoReauthPromoMessageService(
                            MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                            Profile.getLastUsedRegularProfile(), mActivity,
                            SharedPreferencesManager.getInstance(), mIncognitoReauthManager,
                            mSnackbarManager,
                            ()
                                    -> TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mActivity),
                            mLifecycleDispatcher);
                    mMessageCardProviderCoordinator.subscribeMessageService(
                            mIncognitoReauthPromoMessageService);
                }
            }

            mMultiThumbnailCardProvider.initWithNative();
            mMediator.initWithNative(mSnackbarManager);
            // TODO(crbug.com/1222762): Only call setUpPriceTracking in GRID TabSwitcher.
            setUpPriceTracking(mActivity, mModalDialogManager);

            mIsInitialized = true;
        }
    }

    private void initTabSelectionEditor() {
        if (mTabSelectionEditorCoordinator == null) {
            // For tab switcher in carousel mode, the selection editor should still follow grid
            // style.
            int selectionEditorMode = mMode == TabListMode.CAROUSEL ? TabListMode.GRID : mMode;
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(mActivity,
                    mCoordinatorView, mTabModelSelector, mTabContentManager,
                    mTabListCoordinator::setRecyclerViewPosition, selectionEditorMode, mRootView,
                    /*displayGroups=*/true, mTabSelectionEditorSnackbarManager);
            mMediator.setTabSelectionEditorController(
                    mTabSelectionEditorCoordinator.getController());
        }
    }

    private void showTabSelectionEditor() {
        // Lazy initialize if required.
        initTabSelectionEditor();

        if (mTabSelectionEditorActions == null) {
            mTabSelectionEditorActions = new ArrayList<>();
            mTabSelectionEditorActions.add(TabSelectionEditorSelectionAction.createAction(
                    mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.END));
            mTabSelectionEditorActions.add(TabSelectionEditorCloseAction.createAction(
                    mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.START));
            mTabSelectionEditorActions.add(TabSelectionEditorGroupAction.createAction(
                    mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.START));
            mTabSelectionEditorActions.add(TabSelectionEditorBookmarkAction.createAction(
                    mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.START));
            mTabSelectionEditorActions.add(TabSelectionEditorShareAction.createAction(
                    mActivity, ShowMode.MENU_ONLY, ButtonType.ICON_AND_TEXT, IconPosition.START));
        }

        mTabSelectionEditorCoordinator.getController().configureToolbarWithMenuItems(
                mTabSelectionEditorActions,
                new TabSelectionEditorNavigationProvider(
                        mActivity, mTabSelectionEditorCoordinator.getController()));

        List<Tab> tabs = new ArrayList<>();
        TabList list = mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        for (int i = 0; i < list.getCount(); i++) {
            tabs.add(list.getTabAt(i));
        }
        mTabSelectionEditorCoordinator.getController().show(
                tabs, /*preSelectedTabCount=*/0, mTabListCoordinator.getRecyclerViewPosition());
        TabUiMetricsHelper.recordSelectionEditorOpenMetrics(
                TabSelectionEditorOpenMetricGroups.OPEN_FROM_GRID, mActivity);
    }

    private void setUpPriceTracking(Context context, ModalDialogManager modalDialogManager) {
        if (PriceTrackingFeatures.isPriceTrackingEnabled()) {
            PriceDropNotificationManager notificationManager =
                    PriceDropNotificationManagerFactory.create();
            if (mMode == TabListCoordinator.TabListMode.GRID) {
                mPriceMessageService = new PriceMessageService(
                        mTabListCoordinator, mMediator, notificationManager);
                mMessageCardProviderCoordinator.subscribeMessageService(mPriceMessageService);
                mMediator.setPriceMessageService(mPriceMessageService);
            }
        }
    }

    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return this;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        if (mUsesTabGridDialogCoordinator) {
            // mTabGridDialogCoordinator is lazily created when first displaying something in the
            // dialog. Return false until it has shown something.
            return () -> {
                if (mTabGridDialogCoordinator != null) {
                    return mTabGridDialogCoordinator.isVisible();
                }
                return false;
            };
        }
        return () -> false;
    }

    @Override
    public TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return mTabSwitcherCustomViewManager;
    }

    @Override
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    @Override
    public int getTabListTopOffset() {
        return mTabListCoordinator.getTabListTopOffset();
    }

    @Override
    public int getListModeForTesting() {
        return mMode;
    }

    @Override
    public void requestFocusOnCurrentTab() {
        // TODO(crbug.com/1447564): Ideally, this shouldn't be called directly and instead mediator
        // should listen for |requestFocusOnCurrentTab| signal implicitly and apply changes. This
        // would require refactoring TabSwitcher.TabListDelegate and its implementation.
        mMediator.requestAccessibilityFocusOnCurrentTab();
    }

    @Override
    public boolean prepareTabSwitcherView() {
        boolean quick = mMediator.prepareTabSwitcherView();
        mTabListCoordinator.prepareTabSwitcherView();
        return quick;
    }

    @Override
    public void postHiding() {
        mTabListCoordinator.postHiding();
        mMediator.postHiding();
    }

    @Override
    @NonNull
    public Rect getThumbnailLocationOfCurrentTab() {
        if (mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible()) {
            Rect thumbnail = mTabGridDialogCoordinator.getGlobalLocationOfCurrentThumbnail();
            // Adjust to the relative coordinate.
            Rect root = mTabListCoordinator.getRecyclerViewLocation();
            thumbnail.offset(-root.left, -root.top);
            return thumbnail;
        }
        mTabListCoordinator.updateThumbnailLocation();
        return mTabListCoordinator.getThumbnailLocationOfCurrentTab();
    }

    // TabListDelegate implementation.
    @Override
    public int getResourceId() {
        return mTabListCoordinator.getResourceId();
    }

    @Override
    public long getLastDirtyTime() {
        return mTabListCoordinator.getLastDirtyTime();
    }

    @Override
    @VisibleForTesting
    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
        TabListMediator.ThumbnailFetcher.sBitmapCallbackForTesting = callback;
    }

    @Override
    @VisibleForTesting
    public int getBitmapFetchCountForTesting() {
        return TabListMediator.ThumbnailFetcher.sFetchCountForTesting;
    }

    @Override
    @VisibleForTesting
    public int getSoftCleanupDelayForTesting() {
        return mMediator.getSoftCleanupDelayForTesting();
    }

    @Override
    @VisibleForTesting
    public int getCleanupDelayForTesting() {
        return mMediator.getCleanupDelayForTesting();
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode, boolean mruMode) {
        return resetWithTabs(PseudoTab.getListOfPseudoTab(tabList), quickMode, mruMode);
    }

    @Override
    public boolean resetWithTabs(
            @Nullable List<PseudoTab> tabs, boolean quickMode, boolean mruMode) {
        // Invalidate price welcome message for every reset so that the stale message won't be
        // restored by mistake (e.g. from tabClosureUndone in TabSwitcherMediator).
        if (mPriceMessageService != null) {
            mPriceMessageService.invalidateMessage();
        }
        boolean showQuickly = mTabListCoordinator.resetWithListOfTabs(tabs, quickMode, mruMode);
        if (showQuickly) {
            removeAllAppendedMessage();
        }
        if (tabs != null && tabs.size() > 0) {
            if (mPriceMessageService != null
                    && PriceTrackingUtilities.isPriceAlertsMessageCardEnabled()) {
                mPriceMessageService.preparePriceMessage(PriceMessageType.PRICE_ALERTS, null);
            }
            appendMessagesTo(tabs.size());
        }

        return showQuickly;
    }

    // MessageItemsController implementation.
    @Override
    public void removeAllAppendedMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        mTabListCoordinator.removeSpecialListItem(TabProperties.UiType.LARGE_MESSAGE,
                MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        sAppendedMessagesForTesting = false;
    }

    @Override
    public void restoreAllAppendedMessage() {
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (!shouldAppendMessage(messages.get(i).model)) continue;
            // The restore of PRICE_MESSAGE is handled in the restorePriceWelcomeMessage() below.
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                continue;
            } else if (messages.get(i).type
                    == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
                mTabListCoordinator.addSpecialListItemToEnd(
                        TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else {
                mTabListCoordinator.addSpecialListItemToEnd(
                        TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
        }
        sAppendedMessagesForTesting = messages.size() > 0;
    }

    // PriceWelcomeMessageController implementation.
    @Override
    public void removePriceWelcomeMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.PRICE_MESSAGE);
    }

    @Override
    public void restorePriceWelcomeMessage() {
        appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
    }

    @Override
    public void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData) {
        if (mPriceMessageService == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled()
                || mMessageCardProviderCoordinator.isMessageShown(
                        MessageService.MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME)) {
            return;
        }
        if (mPriceMessageService.preparePriceMessage(
                    PriceMessageType.PRICE_WELCOME, priceTabData)) {
            appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
            // To make the message card in view when user enters tab switcher, we should scroll to
            // current tab with 0 offset. See {@link
            // TabSwitcherMediator#setInitialScrollIndexOffset} for more details.
            mMediator.scrollToTab(mTabModelSelector.getTabModelFilterProvider()
                                          .getCurrentTabModelFilter()
                                          .index());
        }
    }

    // OnLongPressTabItemEventListener implementation
    @Override
    public void onLongPressEvent(int tabId) {
        showTabSelectionEditor();
        RecordUserAction.record("TabMultiSelectV2.OpenLongPressInGrid");
    }

    private void appendMessagesTo(int index) {
        if (mMultiWindowModeStateDispatcher.isInMultiWindowMode()) return;
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (!shouldAppendMessage(messages.get(i).model)) continue;
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else if (messages.get(i).type
                    == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
                mayAddIncognitoReauthPromoCard(messages.get(i).model);
            } else {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
            index++;
        }
        if (messages.size() > 0) sAppendedMessagesForTesting = true;
    }

    private void appendNextMessage(@MessageService.MessageType int messageType) {
        assert mMessageCardProviderCoordinator != null;

        MessageCardProviderMediator.Message nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage.model)) return;
        if (messageType == MessageService.MessageType.PRICE_MESSAGE) {
            mTabListCoordinator.addSpecialListItem(
                    mTabListCoordinator.getPriceWelcomeMessageInsertionIndex(),
                    TabProperties.UiType.LARGE_MESSAGE, nextMessage.model);
        } else {
            mTabListCoordinator.addSpecialListItemToEnd(
                    TabProperties.UiType.MESSAGE, nextMessage.model);
        }
    }

    private void mayAddIncognitoReauthPromoCard(PropertyModel model) {
        if (mIncognitoReauthPromoMessageService.isIncognitoReauthPromoMessageEnabled(
                    Profile.getLastUsedRegularProfile())) {
            mTabListCoordinator.addSpecialListItemToEnd(TabProperties.UiType.LARGE_MESSAGE, model);
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        }
    }

    private boolean shouldAppendMessage(PropertyModel messageModel) {
        Integer messageCardVisibilityControlValue = messageModel.get(
                MessageCardViewProperties
                        .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE);

        @MessageCardViewProperties.MessageCardScope
        int scope = (messageCardVisibilityControlValue != null)
                ? messageCardVisibilityControlValue
                : MessageCardViewProperties.MessageCardScope.REGULAR;

        if (scope == MessageCardViewProperties.MessageCardScope.BOTH) return true;
        return mTabModelSelector.isIncognitoSelected()
                ? scope == MessageCardViewProperties.MessageCardScope.INCOGNITO
                : scope == MessageCardViewProperties.MessageCardScope.REGULAR;
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        int index = mTabListCoordinator.indexOfTab(tabId);
        // TODO(crbug.com/999372): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        ViewHolder sourceViewHolder =
                mTabListCoordinator.getContainerView().findViewHolderForAdapterPosition(index);
        if (sourceViewHolder == null) return null;
        return sourceViewHolder.itemView;
    }

    private boolean shouldRegisterMessageItemType() {
        return ChromeFeatureList.sCloseTabSuggestions.isEnabled()
                || (TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mRootView.getContext())
                        && !TabSwitcherCoordinator.isShowingTabsInMRUOrder(mMode));
    }

    private boolean shouldRegisterLargeMessageItemType() {
        return PriceTrackingFeatures.isPriceTrackingEnabled()
                || IncognitoReauthManager.isIncognitoReauthFeatureAvailable();
    }

    @Override
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    @Override
    public void hardCleanup() {
        mTabListCoordinator.hardCleanup();
    }

    // ResetHandler implementation.
    @Override
    public void onDestroy() {
        if (mTabSwitcherMenuActionHandler != null) {
            mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                    mTabSwitcherMenuActionHandler);
        }
        mTabListCoordinator.onDestroy();
        mMessageCardProviderCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        if (mTabGridIphDialogCoordinator != null) {
            mTabGridIphDialogCoordinator.destroy();
        }
        mMultiThumbnailCardProvider.destroy();
        if (mTabSelectionEditorCoordinator != null) {
            mTabSelectionEditorCoordinator.destroy();
        }
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabAttributeCache != null) {
            mTabAttributeCache.destroy();
        }
        if (mPriceAnnotationsPrefObserver != null) {
            SharedPreferencesManager.getInstance().removeObserver(mPriceAnnotationsPrefObserver);
        }
    }

    /**
     * Returns whether tabs should be shown in MRU order in current start surface tab switcher.
     * @param mode The Tab switcher mode.
     */
    static boolean isShowingTabsInMRUOrder(@TabListMode int mode) {
        return StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER.getValue()
                && mode == TabListMode.CAROUSEL;
    }

    @Override
    public void runAnimationOnNextLayout(Runnable runnable) {
        mTabListCoordinator.runAnimationOnNextLayout(runnable);
    }
}
