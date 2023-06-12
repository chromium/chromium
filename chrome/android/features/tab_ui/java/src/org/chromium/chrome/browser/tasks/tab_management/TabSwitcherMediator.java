// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SHADOW_TOP_OFFSET;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The Mediator that is responsible for resetting the tab grid or carousel based on visibility and
 * model changes.
 */
class TabSwitcherMediator implements TabSwitcher.Controller, TabListRecyclerView.VisibilityListener,
                                     TabListMediator.GridCardOnClickListenerProvider,
                                     PriceMessageService.PriceWelcomeMessageReviewActionProvider,
                                     TabSwitcherCustomViewManager.Delegate, BackPressHandler {
    private static final String TAG = "TabSwitcherMediator";

    /** Field trial parameter for the {@link TabListRecyclerView} cleanup delay. */
    private static final String SOFT_CLEANUP_DELAY_PARAM = "soft-cleanup-delay";
    private static final int DEFAULT_SOFT_CLEANUP_DELAY_MS = 3_000;
    private static final String CLEANUP_DELAY_PARAM = "cleanup-delay";
    private static final int DEFAULT_CLEANUP_DELAY_MS = 30_000;

    private final Handler mHandler;
    private final Runnable mSoftClearTabListRunnable;
    private final Runnable mClearTabListRunnable;

    private final ResetHandler mResetHandler;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObserverList<TabSwitcherViewObserver> mObservers = new ObserverList<>();
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final ViewGroup mContainerView;
    private final TabContentManager mTabContentManager;
    private final boolean mIsStartSurfaceEnabled;
    private final boolean mIsStartSurfaceRefactorEnabled;
    private final boolean mIsTablet;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final MultiWindowModeStateDispatcher.MultiWindowModeObserver mMultiWindowModeObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsDialogVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Boolean> mNotifyBackPressedCallback = this::notifyBackPressStateChanged;

    /**
     * The callback which is supplied to the {@link IncognitoReauthController} that takes care of
     * resetting the Incognito tab list with actual Incognito tabs upon successful authentication.
     */
    private final IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallback =
            new IncognitoReauthManager.IncognitoReauthCallback() {
                @Override
                public void onIncognitoReauthNotPossible() {}

                @Override
                public void onIncognitoReauthSuccess() {
                    assert mTabModelSelector.getTabModelFilterProvider()
                            .getCurrentTabModelFilter()
                            .isIncognito()
                        : "The incognito re-auth controller only affects Incognito tab list.";
                    mResetHandler.resetWithTabList(mTabModelSelector.getTabModelFilterProvider()
                                                           .getCurrentTabModelFilter(),
                            false, mShowTabsInMruOrder);
                    setInitialScrollIndexOffset();
                    requestAccessibilityFocusOnCurrentTab();
                }

                @Override
                public void onIncognitoReauthFailure() {}
            };

    private CallbackController mCallbackController;
    private Integer mSoftCleanupDelayMsForTesting;
    private Integer mCleanupDelayMsForTesting;
    private OneshotSupplier<TabGridDialogMediator.DialogController>
            mTabGridDialogControllerSupplier;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;
    private PriceMessageService mPriceMessageService;

    /**
     * This allows to check if re-auth is pending when tab switcher is shown in Incognito mode.
     * If so, we clear the Incognito tab lists until the re-auth is successful.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    /**
     * A custom view that can be supplied by clients to be shown inside the tab grid.
     * Only one client at a time is supported. The custom view is set to null when the client
     * signals the mediator for removal.
     */
    private @Nullable View mCustomView;
    /**
     * A back press {@link Runnable} that can be supplied by clients when adding a custom view.
     */
    private @Nullable Runnable mCustomViewBackPressRunnable;

    /**
     * In cases where a didSelectTab was due to switching models with a toggle,
     * we don't change tab grid visibility.
     */
    private boolean mShouldIgnoreNextSelect;

    private int mModelIndexWhenShown;
    private int mTabIdWhenShown;
    private int mIndexInNewModelWhenSwitched;
    private boolean mIsSelectingInTabSwitcher;

    private boolean mShowTabsInMruOrder;

    private @TabListCoordinator.TabListMode int mMode;
    private Context mContext;
    private SnackbarManager mSnackbarManager;
    private boolean mIsTransitionInProgress;
    private boolean mIsTabSwitcherShowing;

    /**
     * Interface to delegate resetting the tab grid.
     */
    interface ResetHandler {
        /**
         * Reset the tab grid with the given {@link TabList}, which can be null.
         * @param tabList The {@link TabList} to show the tabs for in the grid.
         * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
         * @param mruMode Whether order the Tabs by MRU.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode, boolean mruMode);

        /**
         * Reset the tab grid with the given {@link List<PseudoTab>}, which can be null.
         * @param tabs The {@link List<PseudoTab>} to show the tabs for in the grid.
         * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
         * @param mruMode Whether order the Tabs by MRU.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean resetWithTabs(@Nullable List<PseudoTab> tabs, boolean quickMode, boolean mruMode);

        /**
         * Release the thumbnail {@link Bitmap} but keep the {@link TabGridView}.
         */
        void softCleanup();

        /**
         * Check to see if there are any not viewed price drops when the user leaves the tab
         * switcher. This is done only before the coordinator is destroyed to reduce the amount of
         * calls to ShoppingPersistedTabData.
         */
        void hardCleanup();
    }

    /**
     * Interface to control message items in grid tab switcher.
     */
    interface MessageItemsController {
        /**
         * Remove all the message items in the model list. Right now this is used when all tabs are
         * closed in the grid tab switcher.
         */
        void removeAllAppendedMessage();

        /**
         * Restore all the message items that should show. Right now this is only used to restore
         * message items when the closure of the last tab in tab switcher is undone.
         */
        void restoreAllAppendedMessage();
    }

    /**
     * An interface to control price welcome message in grid tab switcher.
     */
    interface PriceWelcomeMessageController {
        /**
         * Remove the price welcome message item in the model list. Right now this is used when
         * its binding tab is closed in the grid tab switcher.
         */
        void removePriceWelcomeMessage();

        /**
         * Restore the price welcome message item that should show. Right now this is only used
         * when the closure of the binding tab in tab switcher is undone.
         */
        void restorePriceWelcomeMessage();

        /**
         * Show the price welcome message in tab switcher. This is used when any open tab in tab
         * switcher has a price drop.
         */
        void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData);
    }

    /**
     * Basic constructor for the Mediator.
     * @param context The context to use for accessing {@link android.content.res.Resources}.
     * @param resetHandler The {@link ResetHandler} that handles reset for this Mediator.
     * @param containerViewModel The {@link PropertyModel} to keep state on the View containing the
     *         grid or carousel.
     * @param tabModelSelector {@link TabModelSelector} to observer for model and selection changes.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to use.
     * @param containerView The container {@link ViewGroup} to use.
     * @param tabContentManager The {@link TabContentManager} for first meaningful paint event.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} to observe
     *         for multi-window related changes.
     * @param mode One of the {@link TabListMode}.
     * @param incognitoReauthControllerSupplier {@link OneshotSupplier<IncognitoReauthController>}
     *         to detect pending re-auth when tab switcher is shown.
     * @param backPressManager {@link BackPressManager} to handle back press gesture.
     * @param tabGridDialogControllerSupplier {@link TabGridDialogMediator.DialogController}
     *         supplier for lazy initialization on first use.
     */
    TabSwitcherMediator(Context context, ResetHandler resetHandler,
            PropertyModel containerViewModel, TabModelSelector tabModelSelector,
            BrowserControlsStateProvider browserControlsStateProvider, ViewGroup containerView,
            TabContentManager tabContentManager, MessageItemsController messageItemsController,
            PriceWelcomeMessageController priceWelcomeMessageController,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher, @TabListMode int mode,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @Nullable BackPressManager backPressManager,
            @Nullable OneshotSupplier<TabGridDialogMediator.DialogController>
                    tabGridDialogControllerSupplier) {
        mResetHandler = resetHandler;
        mContainerViewModel = containerViewModel;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mMode = mode;
        mContainerViewModel.set(MODE, mode);
        mContext = context;
        mIsStartSurfaceEnabled = ReturnToChromeUtil.isStartSurfaceEnabled(context);
        mIsStartSurfaceRefactorEnabled = ReturnToChromeUtil.isStartSurfaceRefactorEnabled(context);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);

        if (incognitoReauthControllerSupplier != null) {
            mCallbackController = new CallbackController();
            incognitoReauthControllerSupplier.onAvailable(
                    mCallbackController.makeCancelable((incognitoReauthController) -> {
                        mIncognitoReauthController = incognitoReauthController;
                        mIncognitoReauthController.addIncognitoReauthCallback(
                                mIncognitoReauthCallback);
                    }));
        }

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mShouldIgnoreNextSelect = true;
                mIndexInNewModelWhenSwitched = newModel.index();

                TabList currentTabModelFilter =
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
                mContainerViewModel.set(IS_INCOGNITO, currentTabModelFilter.isIncognito());
                notifyBackPressStateChangedInternal();
                if (mTabGridDialogControllerSupplier != null
                        && mTabGridDialogControllerSupplier.hasValue()) {
                    mTabGridDialogControllerSupplier.get().hideDialog(false);
                }
                if (!mContainerViewModel.get(IS_VISIBLE)) return;

                if (clearIncognitoTabListForReauth()) return;
                mResetHandler.resetWithTabList(currentTabModelFilter, false, mShowTabsInMruOrder);
                setInitialScrollIndexOffset();
                requestAccessibilityFocusOnCurrentTab();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState,
                    boolean markedForSelection) {
                // TODO(wychen): move didAddTab and didSelectTab to another observer and inject
                //  after restoreCompleted.
                if (!mTabModelSelector.isTabStateInitialized()) {
                    return;
                }
                mShouldIgnoreNextSelect = false;
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (!mTabModelSelector.isTabStateInitialized()) {
                    return;
                }
                notifyBackPressStateChangedInternal();
                if (type == TabSelectionType.FROM_CLOSE || mShouldIgnoreNextSelect
                        || type == TabSelectionType.FROM_UNDO) {
                    mShouldIgnoreNextSelect = false;
                    return;
                }
                if (mIsSelectingInTabSwitcher) {
                    mIsSelectingInTabSwitcher = false;
                    TabModelFilter modelFilter = mTabModelSelector.getTabModelFilterProvider()
                                                         .getCurrentTabModelFilter();
                    if (modelFilter instanceof TabGroupModelFilter) {
                        ((TabGroupModelFilter) modelFilter).recordSessionsCount(tab);
                    }

                    // Use TabSelectionType.From_USER to filter the new tab creation case.
                    if (type == TabSelectionType.FROM_USER) recordUserSwitchedTab(tab, lastId);
                }

                if (mContainerViewModel.get(IS_VISIBLE)) {
                    onTabSelecting(tab.getId(), false);
                }
            }

            @Override
            public void restoreCompleted() {
                if (!mContainerViewModel.get(IS_VISIBLE)) return;

                mResetHandler.resetWithTabList(
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                        false, mShowTabsInMruOrder);
                setInitialScrollIndexOffset();
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                if (mTabModelSelector.getCurrentModel().getCount() == 1) {
                    messageItemsController.removeAllAppendedMessage();
                } else if (mPriceMessageService != null
                        && mPriceMessageService.getBindingTabId() == tab.getId()) {
                    priceWelcomeMessageController.removePriceWelcomeMessage();
                }
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (mTabModelSelector.getCurrentModel().getCount() == 1) {
                    messageItemsController.restoreAllAppendedMessage();
                }
                if (mPriceMessageService != null
                        && mPriceMessageService.getBindingTabId() == tab.getId()) {
                    priceWelcomeMessageController.restorePriceWelcomeMessage();
                }
                notifyBackPressStateChangedInternal();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                notifyBackPressStateChangedInternal();
            }

            @Override
            public void onFinishingTabClosure(Tab tab) {
                // If tab is closed by the site itself rather than user's input,
                // tabPendingClosure & tabClosureCommitted won't be called.
                notifyBackPressStateChangedInternal();
            }

            @Override
            public void tabRemoved(Tab tab) {
                notifyBackPressStateChangedInternal();
            }

            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                notifyBackPressStateChangedInternal();
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                // TODO(crbug.com/1157578): Auto update the PriceMessageService instead of
                // updating it based on the client caller.
                if (mPriceMessageService != null
                        && mPriceMessageService.getBindingTabId() == tab.getId()) {
                    mPriceMessageService.invalidateMessage();
                }
            }
        };

        mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                if (mMode == TabListCoordinator.TabListMode.CAROUSEL) return;

                updateTopControlsProperties();
            }

            @Override
            public void onTopControlsHeightChanged(
                    int topControlsHeight, int topControlsMinHeight) {
                if (mMode == TabListCoordinator.TabListMode.CAROUSEL) return;

                updateTopControlsProperties();
            }

            @Override
            public void onBottomControlsHeightChanged(
                    int bottomControlsHeight, int bottomControlsMinHeight) {
                mContainerViewModel.set(BOTTOM_CONTROLS_HEIGHT, bottomControlsHeight);
            }
        };

        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);

        if (mTabModelSelector.getModels().isEmpty()) {
            TabModelSelectorObserver selectorObserver = new TabModelSelectorObserver() {
                @Override
                public void onChange() {
                    assert !mTabModelSelector.getModels().isEmpty();
                    assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false)
                            != null;
                    assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true)
                            != null;
                    mTabModelSelector.removeObserver(this);
                    mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(
                            mTabModelObserver);
                }
            };
            mTabModelSelector.addObserver(selectorObserver);
        } else {
            mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(
                    mTabModelObserver);
        }

        mContainerViewModel.set(VISIBILITY_LISTENER, this);
        TabModelFilter tabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        mContainerViewModel.set(
                IS_INCOGNITO, tabModelFilter == null ? false : tabModelFilter.isIncognito());
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);

        // Container view takes care of padding and margin in start surface.
        if (mMode != TabListCoordinator.TabListMode.CAROUSEL) {
            updateTopControlsProperties();
            mContainerViewModel.set(
                    BOTTOM_CONTROLS_HEIGHT, browserControlsStateProvider.getBottomControlsHeight());
        }

        if (mMode == TabListMode.GRID) {
            mContainerViewModel.set(BOTTOM_PADDING,
                    (int) context.getResources().getDimension(R.dimen.tab_grid_bottom_padding));
            if (backPressManager != null && BackPressManager.isEnabled()) {
                assert !mIsStartSurfaceEnabled || mIsStartSurfaceRefactorEnabled;
                backPressManager.addHandler(this, BackPressHandler.Type.TAB_SWITCHER);
                notifyBackPressStateChangedInternal();
            }
        }

        mContainerView = containerView;

        mSoftClearTabListRunnable = mResetHandler::softCleanup;
        mClearTabListRunnable = () -> {
            mResetHandler.hardCleanup();
            mResetHandler.resetWithTabList(null, false, mShowTabsInMruOrder);
        };
        mHandler = new Handler();
        mTabContentManager = tabContentManager;

        mShowTabsInMruOrder = TabSwitcherCoordinator.isShowingTabsInMRUOrder(mMode);

        mMultiWindowModeObserver = isInMultiWindowMode -> {
            if (isInMultiWindowMode) {
                messageItemsController.removeAllAppendedMessage();
            } else {
                messageItemsController.restoreAllAppendedMessage();
            }
        };
        mMultiWindowModeStateDispatcher.addObserver(mMultiWindowModeObserver);
        notifyBackPressStateChangedInternal();

        mTabGridDialogControllerSupplier = tabGridDialogControllerSupplier;
        if (mTabGridDialogControllerSupplier != null) {
            mTabGridDialogControllerSupplier.onAvailable((tabGridDialogController) -> {
                tabGridDialogController.getHandleBackPressChangedSupplier().addObserver(
                        mNotifyBackPressedCallback);
            });
        }
    }

    /**
     * Called after native initialization is completed.
     */
    public void initWithNative(@Nullable SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    /**
     * Initialization of the {@link TabSelectionEditorCoordinator}.
     * @param controller the controller to use.
     */
    public void setTabSelectionEditorController(
            @Nullable TabSelectionEditorCoordinator
                    .TabSelectionEditorController tabSelectionEditorController) {
        if (tabSelectionEditorController != null) {
            mTabSelectionEditorController = tabSelectionEditorController;
            mTabSelectionEditorController.getHandleBackPressChangedSupplier().addObserver(
                    mNotifyBackPressedCallback);
        }
    }

    @VisibleForTesting
    int getSoftCleanupDelayForTesting() {
        return getSoftCleanupDelay();
    }

    private int getSoftCleanupDelay() {
        if (mSoftCleanupDelayMsForTesting != null) return mSoftCleanupDelayMsForTesting;
        if (!LibraryLoader.getInstance().isInitialized()) {
            return 0;
        }

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SOFT_CLEANUP_DELAY_PARAM,
                DEFAULT_SOFT_CLEANUP_DELAY_MS);
    }

    @VisibleForTesting
    int getCleanupDelayForTesting() {
        return getCleanupDelay();
    }

    private int getCleanupDelay() {
        if (mCleanupDelayMsForTesting != null) return mCleanupDelayMsForTesting;
        if (!LibraryLoader.getInstance().isInitialized()) {
            return 0;
        }

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, CLEANUP_DELAY_PARAM,
                DEFAULT_CLEANUP_DELAY_MS);
    }

    private void setVisibility(boolean isVisible) {
        if (isVisible) {
            RecordUserAction.record("MobileToolbarShowStackView");
        }

        mContainerViewModel.set(IS_VISIBLE, isVisible);
        notifyBackPressStateChangedInternal();
    }

    private void updateTopControlsProperties() {
        // If the Start surface is enabled, it will handle the margins and positioning of the tab
        // switcher. So, we shouldn't do it here.
        if (ReturnToChromeUtil.isStartSurfaceEnabled(mContext)
                && !ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mContext)) {
            mContainerViewModel.set(TOP_MARGIN, 0);
            mContainerViewModel.set(SHADOW_TOP_OFFSET, 0);
            return;
        }

        // The grid tab switcher for tablets translates up over top of the browser controls.
        if (mIsTablet) {
            final int toolbarHeight = getToolbarHeight();

            mContainerViewModel.set(TOP_MARGIN, toolbarHeight);
            mContainerViewModel.set(SHADOW_TOP_OFFSET, toolbarHeight);
            return;
        }

        final int contentOffset = mBrowserControlsStateProvider.getContentOffset();

        mContainerViewModel.set(TOP_MARGIN, contentOffset);
        mContainerViewModel.set(SHADOW_TOP_OFFSET, contentOffset);
    }

    /**
     * Record tab switch related metric for GTS.
     * @param tab The new selected tab.
     * @param lastId The id of the previous selected tab, and that tab is still a valid tab
     *               in TabModel.
     */
    private void recordUserSwitchedTab(Tab tab, int lastId) {
        if (tab == null) {
            assert false : "New selected tab cannot be null when recording tab switch.";
            return;
        }

        Tab fromTab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), lastId);
        assert fromTab != null;
        if (mModelIndexWhenShown == mTabModelSelector.getCurrentModelIndex()) {
            if (tab.getId() == mTabIdWhenShown) {
                if (mMode == TabListCoordinator.TabListMode.CAROUSEL) {
                    RecordUserAction.record("MobileTabReturnedToCurrentTab.TabCarousel");
                } else if (mMode == TabListCoordinator.TabListMode.GRID) {
                    RecordUserAction.record("MobileTabReturnedToCurrentTab.TabGrid");
                } else {
                    // TODO(crbug.com/1085246): Differentiate others.
                }
                RecordUserAction.record("MobileTabReturnedToCurrentTab");
                RecordHistogram.recordSparseHistogram(
                        "Tabs.TabOffsetOfSwitch." + TabSwitcherCoordinator.COMPONENT_NAME, 0);
            } else {
                int fromIndex = mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .indexOf(fromTab);
                int toIndex = mTabModelSelector.getTabModelFilterProvider()
                                      .getCurrentTabModelFilter()
                                      .indexOf(tab);

                if (fromIndex != toIndex) {
                    // Only log when you switch a tab page directly from tab switcher.
                    if (getRelatedTabs(tab.getId()).size() == 1) {
                        RecordUserAction.record(
                                "MobileTabSwitched." + TabSwitcherCoordinator.COMPONENT_NAME);
                    }
                    RecordHistogram.recordSparseHistogram(
                            "Tabs.TabOffsetOfSwitch." + TabSwitcherCoordinator.COMPONENT_NAME,
                            fromIndex - toIndex);
                }
            }
        } else {
            int newSelectedTabIndex =
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tab.getId());
            if (newSelectedTabIndex == mIndexInNewModelWhenSwitched) {
                // TabModelImpl logs this action only when a different index is set within a
                // TabModelImpl. If we switch between normal tab model and incognito tab model and
                // leave the index the same (i.e. after switched tab model and select the
                // highlighted tab), TabModelImpl doesn't catch this case. Therefore, we record it
                // here.
                RecordUserAction.record("MobileTabSwitched");
            }
            // Only log when you switch a tab page directly from tab switcher.
            if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)
                    || getRelatedTabs(tab.getId()).size() == 1) {
                RecordUserAction.record(
                        "MobileTabSwitched." + TabSwitcherCoordinator.COMPONENT_NAME);
            }
        }
        if (mMode == TabListCoordinator.TabListMode.GRID
                && !mTabModelSelector.getCurrentModel().getProfile().isOffTheRecord()
                && PriceTrackingUtilities.isTrackPricesOnTabsEnabled()) {
            RecordUserAction.record("Commerce.TabGridSwitched."
                    + (ShoppingPersistedTabData.hasPriceDrop(tab) ? "HasPriceDrop"
                                                                  : "NoPriceDrop"));
        }
    }

    @Override
    public boolean overviewVisible() {
        return mContainerViewModel.get(IS_VISIBLE);
    }

    @Override
    public ViewGroup getTabSwitcherContainer() {
        return mContainerView;
    }

    @Override
    public void addTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void prepareHideTabSwitcherView() {
        if (mTabGridDialogControllerSupplier != null
                && mTabGridDialogControllerSupplier.hasValue()) {
            // Don't wait until switcher container view hides.
            // Hide dialog before GTS hides.
            mTabGridDialogControllerSupplier.get().hideDialog(false);
        }
    }

    @Override
    public void hideTabSwitcherView(boolean animate) {
        if (mMode == TabListMode.GRID) {
            mIsTransitionInProgress = true;
            notifyBackPressStateChangedInternal();
        }

        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(false);
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);

        if (mTabGridDialogControllerSupplier != null
                && mTabGridDialogControllerSupplier.hasValue()) {
            // Don't wait until didSelectTab(), which is after the GTS animation.
            // We need to hide the dialog immediately.
            mTabGridDialogControllerSupplier.get().hideDialog(false);
        }
    }

    boolean prepareTabSwitcherView() {
        mHandler.removeCallbacks(mSoftClearTabListRunnable);
        mHandler.removeCallbacks(mClearTabListRunnable);
        boolean quick = false;
        if (!mTabModelSelector.isTabStateInitialized()) return quick;
        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext)) {
            quick = mResetHandler.resetWithTabList(
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(), false,
                    mShowTabsInMruOrder);
        }
        setInitialScrollIndexOffset();

        return quick;
    }

    private void setInitialScrollIndexOffset() {
        int initialPosition =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter().index();

        // In MRU order, selected Tab is always at the first position.
        if (mShowTabsInMruOrder) initialPosition = 0;
        mContainerViewModel.set(INITIAL_SCROLL_INDEX, initialPosition);
    }

    @Override
    public void prepareShowTabSwitcherView() {
        if (mMode != TabListMode.GRID) return;

        mIsTransitionInProgress = true;
        notifyBackPressStateChangedInternal();
    }

    @Override
    public void showTabSwitcherView(boolean animate) {
        mIsTransitionInProgress = false;
        mHandler.removeCallbacks(mSoftClearTabListRunnable);
        mHandler.removeCallbacks(mClearTabListRunnable);

        if (!mTabModelSelector.isIncognitoSelected() || !clearIncognitoTabListForReauth()) {
            if (mTabModelSelector.isTabStateInitialized()) {
                mResetHandler.resetWithTabList(
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                        TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext),
                        mShowTabsInMruOrder);
                // When |mTabModelSelector.isTabStateInitialized| is false and INSTANT_START is
                // enabled, the scrolling request is already processed in
                // TabModelObserver#restoreCompleted. Therefore, we only need to handle the case
                // with isTabStateInitialized() here.
                setInitialScrollIndexOffset();
            } else if (ChromeFeatureList.sInstantStart.isEnabled()) {
                List<PseudoTab> allTabs;
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    allTabs = PseudoTab.getAllPseudoTabsFromStateFile(mContext);
                }
                mResetHandler.resetWithTabs(allTabs,
                        TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext),
                        mShowTabsInMruOrder);
            }
        }

        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(true);
        mModelIndexWhenShown = mTabModelSelector.getCurrentModelIndex();
        mTabIdWhenShown = mTabModelSelector.getCurrentTabId();
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    @Override
    public void startedShowing(boolean isAnimating) {
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        mIsTabSwitcherShowing = true;

        if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(mContext)) {
            requestAccessibilityFocusOnCurrentTab();
        }

        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding(boolean isAnimating) {
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        mIsTabSwitcherShowing = false;
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    @Override
    public boolean onBackPressed() {
        boolean ret = onBackPressedInternal();
        if (ret && (!mIsStartSurfaceEnabled || mIsStartSurfaceRefactorEnabled)) {
            // When SS is enabled or refactor is disabled, StartSurfaceMediator will consume back
            // press and call this method if necessary.
            BackPressManager.record(BackPressHandler.Type.TAB_SWITCHER);
        }
        return ret;
    }

    private boolean onBackPressedInternal() {
        // The TabSelectionEditor dialog can be shown on the Start surface without showing the Grid
        // Tab switcher, so skip the check of visibility of mContainerViewModel here.
        if (mTabSelectionEditorController != null
                && mTabSelectionEditorController.handleBackPressed()) {
            return true;
        }

        if (mCustomViewBackPressRunnable != null) {
            mCustomViewBackPressRunnable.run();
            return true;
        }

        if (!mIsTablet && mIsTransitionInProgress && mMode == TabListCoordinator.TabListMode.GRID) {
            // crbug.com/1420410: intentionally do nothing to wait for tab-to-GTS transition to be
            // finished. Note this has to be before following if-branch since during transition, the
            // container is still invisible. On tablet, the translation transition replaces the
            // tab-to-GTS (expand/shrink) animation, which does not suffer from the same issue.
            return true;
        }

        if (!mContainerViewModel.get(IS_VISIBLE)) {
            assert !BackPressManager.isEnabled() : "Invisible container: Backpress must be handled";
            return false;
        }

        if (mTabGridDialogControllerSupplier != null && mTabGridDialogControllerSupplier.hasValue()
                && mTabGridDialogControllerSupplier.get().handleBackPressed()) {
            return true;
        }

        // When the Start surface is showing, we no longer need to call onTabSelecting().
        if (mMode == TabListCoordinator.TabListMode.CAROUSEL) return false;

        if (mTabModelSelector.getCurrentTab() == null) {
            assert !BackPressManager.isEnabled() : "No tab: Backpress must be handled";
            return false;
        }

        onTabSelecting(mTabModelSelector.getCurrentTabId(), false);

        return true;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressedInternal() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public boolean isDialogVisible() {
        if (mTabSelectionEditorController != null && mTabSelectionEditorController.isVisible()) {
            return true;
        }

        if (mTabGridDialogControllerSupplier != null && mTabGridDialogControllerSupplier.hasValue()
                && mTabGridDialogControllerSupplier.get().isVisible()) {
            return true;
        }
        return false;
    }

    @Override
    public ObservableSupplier<Boolean> isDialogVisibleSupplier() {
        return mIsDialogVisibleSupplier;
    }

    @Override
    public void onOverviewShownAtLaunch(long activityCreationTimeMs) {}

    @Override
    public @TabSwitcherType int getTabSwitcherType() {
        switch (mMode) {
            case TabListMode.CAROUSEL:
                return TabSwitcherType.CAROUSEL;
            case TabListMode.GRID:
                return TabSwitcherType.GRID;
            case TabListMode.LIST:
            case TabListMode.STRIP:
            default:
                return TabSwitcherType.NONE;
        }
    }

    @Override
    public void onHomepageChanged() {
        notifyBackPressStateChangedInternal();
    }

    @Override
    public void setSnackbarParentView(ViewGroup parentView) {
        if (mSnackbarManager == null) return;
        mSnackbarManager.setParentView(parentView);
    }

    /**
     * A method to handle signal from outside world that a client is requesting to show a custom
     * view inside the tab switcher.
     *
     * @param customView        A {@link View} view that needs to be shown.
     * @param backPressRunnable A {@link Runnable} which can be supplied if clients also wish to
     *                          handle back presses while the custom view is shown. A null value
     *                          can be passed to not
     *                          intercept back presses.
     * @param clearTabList      A boolean to indicate whether we should clear the tab list when
     *                          showing the custom view.
     */
    @Override
    public void addCustomView(
            @NonNull View customView, @Nullable Runnable backPressRunnable, boolean clearTabList) {
        assert mCustomView == null : "Only one client at a time is supported to add a custom view.";

        // Hide any tab grid dialog before we add the custom view.
        if (mTabGridDialogControllerSupplier != null
                && mTabGridDialogControllerSupplier.hasValue()) {
            mTabGridDialogControllerSupplier.get().hideDialog(false);
        }

        if (clearTabList) {
            mResetHandler.resetWithTabList(null, false, mShowTabsInMruOrder);
        }

        // The grid tab switcher for tablets translates up over top of the browser controls, causing
        // the custom view to do the same.
        if (mIsTablet) {
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);
            params.topMargin = getToolbarHeight();

            mContainerView.addView(customView, params);
        } else {
            mContainerView.addView(customView);
        }

        mCustomView = customView;
        mCustomViewBackPressRunnable = backPressRunnable;
        notifyBackPressStateChangedInternal();
    }

    /**
     * A method to handle signal from outside world that a client is requesting to remove the custom
     * view from the tab switcher.
     *
     * @param customView A {@link View} view that needs to be removed.
     */
    @Override
    public void removeCustomView(@NonNull View customView) {
        assert mCustomView
                != null : "No client previously passed a custom view that needs removal.";
        mContainerView.removeView(customView);
        mCustomView = null;
        mCustomViewBackPressRunnable = null;
        notifyBackPressStateChangedInternal();
        mResetHandler.resetWithTabList(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                /*quickMode=*/false, mShowTabsInMruOrder);
    }

    /**
     * Do clean-up work after the overview hiding animation is finished.
     * @see TabSwitcher.TabListDelegate#postHiding
     */
    void postHiding() {
        Log.d(TAG, "SoftCleanupDelay = " + getSoftCleanupDelay());
        mHandler.postDelayed(mSoftClearTabListRunnable, getSoftCleanupDelay());
        Log.d(TAG, "CleanupDelay = " + getCleanupDelay());
        mHandler.postDelayed(mClearTabListRunnable, getCleanupDelay());
        mIsTransitionInProgress = false;
        notifyBackPressStateChangedInternal();
    }

    /**
     * Set the delay for soft cleanup.
     */
    void setSoftCleanupDelayForTesting(int timeoutMs) {
        mSoftCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Set the delay for lazy cleanup.
     */
    void setCleanupDelayForTesting(int timeoutMs) {
        mCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        if (mTabSelectionEditorController != null) {
            mTabSelectionEditorController.getHandleBackPressChangedSupplier().removeObserver(
                    mNotifyBackPressedCallback);
        }

        if (mTabGridDialogControllerSupplier != null
                && mTabGridDialogControllerSupplier.hasValue()) {
            mTabGridDialogControllerSupplier.get()
                    .getHandleBackPressChangedSupplier()
                    .removeObserver(mNotifyBackPressedCallback);
        }

        if (mIncognitoReauthController != null) {
            mIncognitoReauthController.removeIncognitoReauthCallback(mIncognitoReauthCallback);
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
        }

        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
        mMultiWindowModeStateDispatcher.removeObserver(mMultiWindowModeObserver);
    }

    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mOnTabSelectingListener = listener;
    }

    void setPriceMessageService(PriceMessageService priceMessageService) {
        mPriceMessageService = priceMessageService;
    }

    void requestAccessibilityFocusOnCurrentTab() {
        if (!mIsTabSwitcherShowing || !mTabModelSelector.isTabStateInitialized()) {
            return;
        }

        if (mTabModelSelector.isIncognitoSelected() && mIncognitoReauthController != null
                && mIncognitoReauthController.isReauthPageShowing()) {
            return;
        }

        mContainerViewModel.set(TabListContainerProperties.FOCUS_TAB_INDEX_FOR_ACCESSIBILITY,
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter().index());
    }

    // GridCardOnClickListenerProvider implementation.
    @Override
    @Nullable
    public TabListMediator.TabActionListener openTabGridDialog(Tab tab) {
        if (!ableToOpenDialog(tab)) return null;
        assert getRelatedTabs(tab.getId()).size() != 1;
        assert mTabGridDialogControllerSupplier != null;
        return tabId -> {
            List<Tab> relatedTabs = getRelatedTabs(tabId);
            if (relatedTabs.size() == 0) {
                relatedTabs = null;
            }
            mTabGridDialogControllerSupplier.get().resetWithListOfTabs(relatedTabs);
            RecordUserAction.record("TabGridDialog.ExpandedFromSwitcher");
        };
    }

    @Override
    public void onTabSelecting(int tabId, boolean fromActionButton) {
        if (fromActionButton && (mMode == TabListMode.CAROUSEL || mMode == TabListMode.GRID)) {
            Tab newlySelectedTab =
                    TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);
            StartSurfaceUserData.setKeepTab(newlySelectedTab, true);
            if (mMode == TabListMode.CAROUSEL) {
                StartSurfaceUserData.setOpenedFromStart(newlySelectedTab);
            }
        }
        mIsSelectingInTabSwitcher = true;
        if (mOnTabSelectingListener != null) {
            mOnTabSelectingListener.onTabSelecting(LayoutManagerImpl.time(), tabId);
        }
    }

    @Override
    public void scrollToTab(int tabIndex) {
        mContainerViewModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, tabIndex);
    }

    private boolean ableToOpenDialog(Tab tab) {
        return TabUiFeatureUtilities.isTabGroupsAndroidEnabled(mContext)
                && mTabModelSelector.isIncognitoSelected() == tab.isIncognito()
                && getRelatedTabs(tab.getId()).size() != 1;
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }

    private void notifyBackPressStateChanged(boolean noop) {
        notifyBackPressStateChangedInternal();
    }

    private void notifyBackPressStateChangedInternal() {
        mIsDialogVisibleSupplier.set(isDialogVisible());
        mBackPressChangedSupplier.set(shouldInterceptBackPress());
    }

    private int getToolbarHeight() {
        return mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
    }

    /**
     * A method to indicate whether back press should be intercepted. The respective interceptors
     * should also take care of invoking #notifyBackPressStateChangedInternal each time their
     * decision to intercept back press has changed.
     *
     * @return A boolean to indicate if back press should be intercepted or not.
     */
    @VisibleForTesting
    boolean shouldInterceptBackPress() {
        if (isDialogVisible()) return true;
        if (mCustomViewBackPressRunnable != null) return true;

        if (!mIsTablet && mIsTransitionInProgress && mMode == TabListCoordinator.TabListMode.GRID) {
            return true;
        }

        if (!mContainerViewModel.get(IS_VISIBLE)) return false;

        // When the Start surface is showing, we no longer need to call onTabSelecting().
        if (mMode == TabListCoordinator.TabListMode.CAROUSEL) return false;

        if (mTabModelSelector.getCurrentTab() == null) return false;

        return true;
    }

    /**
     * A method which clears the Incognito tab lists when a re-auth is pending.
     *
     * @return True, if the Incognito tab list was requested to be cleared and false otherwise.
     */
    private boolean clearIncognitoTabListForReauth() {
        if (!mTabModelSelector.isIncognitoSelected()) return false;

        if (mIncognitoReauthController != null
                && mIncognitoReauthController.isIncognitoReauthPending()) {
            mResetHandler.resetWithTabList(null, false, mShowTabsInMruOrder);
            return true;
        }

        return false;
    }
}
