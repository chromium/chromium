// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabKeyEventHandler.onPageKeyEvent;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.PAGE_KEY_LISTENER;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.TabGridContextMenuCoordinator.ShowTabListEditor;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;
import java.util.Set;

/** Coordinator for a {@link TabSwitcherPaneBase}'s UI. */
public class TabSwitcherPaneCoordinator implements BackPressHandler {
    static final String COMPONENT_NAME = "GridTabSwitcher";

    private final MessageUpdateObserver mMessageUpdateObserver =
            new MessageUpdateObserver() {
                @Override
                public void onAppendedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemovedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemoveAllAppendedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRestoreAllAppendedMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onShowPriceWelcomeMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRemovePriceWelcomeMessage() {
                    updateBottomPadding();
                }

                @Override
                public void onRestorePriceWelcomeMessage() {
                    updateBottomPadding();
                }
            };

    private final TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener
            mLongPressItemEventListener = this::onLongPressOnTabCard;
    private final Activity mActivity;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final Callback<Boolean> mOnVisibilityChanged = this::onVisibilityChanged;
    private final ObservableSupplier<Boolean> mIsVisibleSupplier;
    private final ObservableSupplier<Boolean> mIsAnimatingSupplier;
    private final TabSwitcherPaneMediator mMediator;
    private final Supplier<Boolean> mTabGridDialogVisibilitySupplier = this::isTabGridDialogVisible;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabListCoordinator mTabListCoordinator;
    private final PropertyModel mContainerViewModel;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final LazyOneshotSupplier<DialogController> mDialogControllerSupplier;
    private final TabListEditorManager mTabListEditorManager;
    private final ViewGroup mParentView;
    private final TabSwitcherMessageManager mMessageManager;
    private final ModalDialogManager mModalDialogManager;
    private final BottomSheetController mBottomSheetController;
    private final Runnable mOnDestroyed;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final TabListOnScrollListener mTabListOnScrollListener = new TabListOnScrollListener();
    private final OneshotSupplierImpl<ObservableSupplier<Boolean>> mIsScrollingSupplier =
            new OneshotSupplierImpl<>();
    private final Callback<EdgeToEdgeController> mOnEdgeToEdgeControllerChangedCallback =
            new ValueChangedCallback<>(this::onEdgeToEdgeControllerChanged);
    private final @Nullable TabGroupLabeller mTabGroupLabeller;
    private final ObservableSupplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final Runnable mOnTabGroupCreation;
    private final Callback<TabGroupModelFilter> mOnFilterChange = this::onFilterChange;
    private @Nullable TabGridContextMenuCoordinator mContextMenuCoordinator;
    private @Nullable TabGroupListBottomSheetCoordinator mTabGroupListBottomSheetCoordinator;

    /** Lazily initialized when shown. */
    private @Nullable TabGridDialogCoordinator mTabGridDialogCoordinator;

    private @Nullable Function<Integer, View> mFetchViewByIndex;
    private @Nullable Supplier<Pair<Integer, Integer>> mGetVisibleIndex;

    /** Not null when drawing the hub edge to edge. */
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    private int mEdgeToEdgeBottomInsets;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabGroupModelFilterSupplier The supplier of the tab model filter fo rthis pane.
     * @param tabContentManager For management of thumbnails.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param scrimManager The scrim component to use for the tab grid dialog.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param messageManager The {@link TabSwitcherMessageManager} for the message service.
     * @param parentView The view to use as a parent.
     * @param resetHandler The tab list reset handler for the pane.
     * @param isVisibleSupplier The supplier of the pane's visibility.
     * @param isAnimatingSupplier Whether the pane is animating into or out of view.
     * @param onTabClickCallback Callback to invoke when a tab is clicked.
     * @param setHairlineVisibilityCallback Callback to be invoked to show or hide the hairline.
     * @param mode The {@link TabListMode} to use.
     * @param supportsEmptyState Whether empty state UI should be shown when the model is empty.
     * @param onTabGroupCreation Should be run when the UI is used to create a tab group.
     * @param onDestroyed A {@link Runnable} to execute when {@link #destroy()} is invoked.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     */
    public TabSwitcherPaneCoordinator(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull ObservableSupplier<TabGroupModelFilter> tabGroupModelFilterSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ScrimManager scrimManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            @NonNull TabSwitcherMessageManager messageManager,
            @NonNull ViewGroup parentView,
            @NonNull TabSwitcherResetHandler resetHandler,
            @NonNull ObservableSupplier<Boolean> isVisibleSupplier,
            @NonNull ObservableSupplier<Boolean> isAnimatingSupplier,
            @NonNull Callback<Integer> onTabClickCallback,
            @NonNull Callback<Boolean> setHairlineVisibilityCallback,
            @TabListMode int mode,
            boolean supportsEmptyState,
            @Nullable Runnable onTabGroupCreation,
            @NonNull Runnable onDestroyed,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            UndoBarThrottle undoBarThrottle) {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherPaneCoordinator.constructor")) {
            mProfileProviderSupplier = profileProviderSupplier;
            mIsVisibleSupplier = isVisibleSupplier;
            mIsAnimatingSupplier = isAnimatingSupplier;
            mActivity = activity;
            mModalDialogManager = modalDialogManager;
            mBottomSheetController = bottomSheetController;
            mParentView = parentView;
            mOnDestroyed = onDestroyed;
            mEdgeToEdgeSupplier = edgeToEdgeSupplier;
            mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
            mOnTabGroupCreation = onTabGroupCreation;
            mShareDelegateSupplier = shareDelegateSupplier;
            mTabBookmarkerSupplier = tabBookmarkerSupplier;

            assert mode != TabListMode.STRIP : "TabListMode.STRIP not supported.";

            ViewGroup coordinatorView = activity.findViewById(R.id.coordinator);

            PropertyModel containerViewModel =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(BROWSER_CONTROLS_STATE_PROVIDER, browserControlsStateProvider)
                            .with(MODE, mode)
                            .with(FETCH_VIEW_BY_INDEX_CALLBACK, (f) -> mFetchViewByIndex = f)
                            .with(GET_VISIBLE_RANGE_CALLBACK, (f) -> mGetVisibleIndex = f)
                            .with(
                                    IS_SCROLLING_SUPPLIER_CALLBACK,
                                    (f) -> mIsScrollingSupplier.set(f))
                            .with(
                                    PAGE_KEY_LISTENER,
                                    event ->
                                            onPageKeyEvent(
                                                    event,
                                                    mTabGroupModelFilterSupplier.get(),
                                                    /* moveSingleTab= */ false))
                            .build();

            mContainerViewModel = containerViewModel;
            Profile profile = mProfileProviderSupplier.get().getOriginalProfile();
            mDialogControllerSupplier =
                    LazyOneshotSupplier.fromSupplier(
                            () -> {
                                mTabGridDialogCoordinator =
                                        new TabGridDialogCoordinator(
                                                activity,
                                                browserControlsStateProvider,
                                                bottomSheetController,
                                                dataSharingTabManager,
                                                tabGroupModelFilterSupplier,
                                                tabContentManager,
                                                coordinatorView,
                                                resetHandler,
                                                getGridCardOnClickListenerProvider(),
                                                TabSwitcherPaneCoordinator.this
                                                        ::getTabGridDialogAnimationSourceView,
                                                scrimManager,
                                                mModalDialogManager,
                                                desktopWindowStateManager,
                                                undoBarThrottle,
                                                tabBookmarkerSupplier,
                                                shareDelegateSupplier);
                                mTabGridDialogCoordinator.setPageKeyEvent(
                                        event ->
                                                onPageKeyEvent(
                                                        event,
                                                        mTabGroupModelFilterSupplier.get(),
                                                        /* moveSingleTab= */ true));
                                return mTabGridDialogCoordinator.getDialogController();
                            });

            mMediator =
                    new TabSwitcherPaneMediator(
                            resetHandler,
                            tabGroupModelFilterSupplier,
                            mDialogControllerSupplier,
                            containerViewModel,
                            parentView,
                            this::onTabSwitcherShown,
                            isVisibleSupplier,
                            isAnimatingSupplier,
                            onTabClickCallback,
                            this::getNthTabIndexInModel);

            mMultiThumbnailCardProvider =
                    new MultiThumbnailCardProvider(
                            activity,
                            browserControlsStateProvider,
                            tabContentManager,
                            tabGroupModelFilterSupplier);

            var recyclerViewTimer = new UptimeMillisTimer();

            @DrawableRes
            int emptyImageResId =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(activity)
                            ? R.drawable.tablet_tab_switcher_empty_state_illustration
                            : R.drawable.phone_tab_switcher_empty_state_illustration;
            TabListCoordinator tabListCoordinator =
                    new TabListCoordinator(
                            mode,
                            activity,
                            browserControlsStateProvider,
                            mModalDialogManager,
                            tabGroupModelFilterSupplier,
                            mMultiThumbnailCardProvider,
                            /* actionOnRelatedTabs= */ true,
                            dataSharingTabManager,
                            getGridCardOnClickListenerProvider(),
                            /* dialogHandler= */ null,
                            TabProperties.TabActionState.CLOSABLE,
                            /* selectionDelegateProvider= */ null,
                            this::getPriceWelcomeMessageController,
                            parentView,
                            /* attachToParent= */ true,
                            COMPONENT_NAME,
                            /* onModelTokenChange= */ null,
                            /* hasEmptyView= */ supportsEmptyState,
                            supportsEmptyState ? emptyImageResId : Resources.ID_NULL,
                            supportsEmptyState
                                    ? R.string.tabswitcher_no_tabs_empty_state
                                    : Resources.ID_NULL,
                            supportsEmptyState
                                    ? R.string.tabswitcher_no_tabs_open_to_visit_different_pages
                                    : Resources.ID_NULL,
                            onTabGroupCreation,
                            /* allowDragAndDrop= */ true);
            mTabListCoordinator = tabListCoordinator;
            tabListCoordinator.setOnLongPressTabItemEventListener(mLongPressItemEventListener);

            mTabListOnScrollListener
                    .getYOffsetNonZeroSupplier()
                    .addObserver(setHairlineVisibilityCallback);
            TabListRecyclerView recyclerView = tabListCoordinator.getContainerView();
            recyclerView.setVisibility(View.VISIBLE);
            recyclerView.setBackgroundColor(Color.TRANSPARENT);
            recyclerView.addOnScrollListener(mTabListOnScrollListener);
            mContainerViewChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            containerViewModel, recyclerView, TabListContainerViewBinder::bind);

            if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
                mEdgeToEdgePadAdjuster =
                        new EdgeToEdgePadAdjuster() {
                            @Override
                            public void overrideBottomInset(int inset) {
                                mEdgeToEdgeBottomInsets = inset;
                                updateBottomPadding();
                            }

                            @Override
                            public void destroy() {}
                        };
                mEdgeToEdgeSupplier.addObserver(mOnEdgeToEdgeControllerChangedCallback);
            }

            RecordHistogram.recordTimesHistogram(
                    "Android.TabSwitcher.SetupRecyclerView.Time",
                    recyclerViewTimer.getElapsedMillis());

            TabListEditorManager tabListEditorManager =
                    new TabListEditorManager(
                            activity,
                            mModalDialogManager,
                            coordinatorView,
                            /* rootView= */ parentView,
                            browserControlsStateProvider,
                            tabGroupModelFilterSupplier,
                            tabContentManager,
                            tabListCoordinator,
                            bottomSheetController,
                            mode,
                            onTabGroupCreation,
                            desktopWindowStateManager,
                            mEdgeToEdgeSupplier);
            mTabListEditorManager = tabListEditorManager;
            mMediator.setTabListEditorControllerSupplier(
                    mTabListEditorManager.getControllerSupplier());

            mMessageManager = messageManager;
            mMessageManager.registerMessages(tabListCoordinator);

            CollaborationService collaborationService =
                    CollaborationServiceFactory.getForProfile(profile);
            @NonNull ServiceStatus serviceStatus = collaborationService.getServiceStatus();
            if (serviceStatus.isAllowedToJoin()) {
                mTabGroupLabeller =
                        new TabGroupLabeller(
                                profile,
                                mTabListCoordinator.getTabListNotificationHandler(),
                                tabGroupModelFilterSupplier);
            } else {
                mTabGroupLabeller = null;
            }

            mOnVisibilityChanged.onResult(isVisibleSupplier.addObserver(mOnVisibilityChanged));
            mTabGroupModelFilterSupplier.addObserver(mOnFilterChange);
        }
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mMessageManager.removeObserver(mMessageUpdateObserver);
        mMessageManager.unbind(mTabListCoordinator);
        mMediator.destroy();
        mTabListCoordinator.onDestroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mIsVisibleSupplier.removeObserver(mOnVisibilityChanged);
        mMultiThumbnailCardProvider.destroy();
        mTabListEditorManager.destroy();
        mOnDestroyed.run();
        mEdgeToEdgeSupplier.removeObserver(mOnEdgeToEdgeControllerChangedCallback);
        if (mEdgeToEdgePadAdjuster != null && mEdgeToEdgeSupplier.get() != null) {
            mEdgeToEdgeSupplier.get().unregisterAdjuster(mEdgeToEdgePadAdjuster);
            mEdgeToEdgePadAdjuster = null;
        }
        if (mTabGroupLabeller != null) {
            mTabGroupLabeller.destroy();
        }
        mTabGroupModelFilterSupplier.removeObserver(mOnFilterChange);
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
        }
    }

    /** Post native initialization. */
    public void initWithNative() {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherPaneCoordinator.initWithNative")) {
            ProfileProvider profileProvider = mProfileProviderSupplier.get();
            assert profileProvider != null;
            Profile originalProfile = profileProvider.getOriginalProfile();
            mTabListCoordinator.initWithNative(originalProfile);
            mMultiThumbnailCardProvider.initWithNative(originalProfile);
        }
    }

    /** Shows the tab list editor. */
    public void showTabListEditor() {
        mTabListEditorManager.showTabListEditor();
    }

    /**
     * Resets the UI with the specified tabs.
     *
     * @param tabList The {@link TabList} to show tabs for.
     */
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mMessageManager.beforeReset();
        // Quick mode being false here ensures the selected tab's thumbnail gets updated. With Hub
        // the TabListCoordinator no longer triggers thumbnail captures so this shouldn't guard
        // against the large amount of work that is used to.
        mTabListCoordinator.resetWithListOfTabs(
                tabs, /* tabGroupSyncIds= */ null, /* quickMode= */ false);
        mMessageManager.afterReset(tabs == null ? 0 : tabs.size());
        mTabListOnScrollListener.postUpdate(mTabListCoordinator.getContainerView());
        if (mTabGroupLabeller != null) {
            mTabGroupLabeller.showAll();
        }
    }

    /** Performs soft cleanup which removes thumbnails to relieve memory usage. */
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    /** Performs hard cleanup which saves price drop information. */
    public void hardCleanup() {
        // TODO(crbug.com/40946413): The pre-fork implementation resets the tab list, this seems
        // suboptimal. Consider not doing this.
        resetWithListOfTabs(null);
    }

    /**
     * Scrolls so that the selected tab in the current model is in the middle of the screen or as
     * close as possible if at the start/end of the recycler view.
     */
    public void setInitialScrollIndexOffset() {
        mMediator.setInitialScrollIndexOffset();
    }

    /** Requests accessibility focus on the current tab. */
    public void requestAccessibilityFocusOnCurrentTab() {
        mMediator.requestAccessibilityFocusOnCurrentTab();
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public @NonNull Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogVisibilitySupplier;
    }

    /** Provides information on whether the tab grid dialog is showing or animating. */
    public @Nullable ObservableSupplier<Boolean> getTabGridDialogShowingOrAnimationSupplier() {
        return mTabGridDialogCoordinator != null
                ? mTabGridDialogCoordinator.getShowingOrAnimationSupplier()
                : null;
    }

    /** Provides the tab ID for the most recently swiped tab. */
    public @NonNull ObservableSupplier<Integer> getRecentlySwipedTabIdSupplier() {
        return mTabListCoordinator.getRecentlySwipedTabSupplier();
    }

    /** Returns whether the TabListEditor needs a clean up. */
    public boolean doesTabListEditorNeedCleanup() {
        @Nullable TabListEditorController controller = mMediator.getTabListEditorController();
        return controller != null && controller.needsCleanUp();
    }

    /** Returns a {@link TabSwitcherCustomViewManager.Delegate} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager.Delegate
            getTabSwitcherCustomViewManagerDelegate() {
        return mMediator;
    }

    /** Indicates whether any animator for the {@link TabListRecyclerView} is running. */
    public @Nullable ObservableSupplier<Boolean> getIsRecyclerViewAnimatorRunning() {
        TabListRecyclerView containerView = mTabListCoordinator.getContainerView();
        if (containerView == null) {
            return null;
        }
        return containerView.getIsAnimatorRunningSupplier();
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        return mTabListCoordinator.getTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        mTabListCoordinator.setRecyclerViewPosition(position);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public @NonNull Rect getRecyclerViewRect() {
        return mTabListCoordinator.getRecyclerViewLocation();
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    public @NonNull Rect getTabThumbnailRect(int tabId) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getTabThumbnailRect(tabId);
        }
        return mTabListCoordinator.getTabThumbnailRect(tabId);
    }

    /** Returns the {@link Rect} of the recyclerview in global coordinates. */
    public @NonNull Size getThumbnailSize() {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            return dialogCoordinator.getThumbnailSize();
        }
        return mTabListCoordinator.getThumbnailSize();
    }

    /**
     * @param tabId The tab ID whose view to wait for.
     * @param r A runnable to be executed on the next layout pass or immediately if one is not
     *     scheduled.
     */
    public void waitForLayoutWithTab(int tabId, Runnable r) {
        TabGridDialogCoordinator dialogCoordinator = mTabGridDialogCoordinator;
        if (dialogCoordinator != null && dialogCoordinator.isVisible()) {
            dialogCoordinator.waitForLayoutWithTab(tabId, r);
            return;
        }
        mTabListCoordinator.waitForLayoutWithTab(tabId, r);
    }

    /**
     * Scrolls to the specified group and animates open a dialog. It is the caller's responsibility
     * to ensure that this pane is showing before calling this.
     *
     * @param tabId The id of any tab in the group.
     */
    public void requestOpenTabGroupDialog(int tabId) {
        mMediator.scrollToTabById(tabId);
        mMediator.openTabGroupDialog(tabId);
    }

    /** Returns the range (inclusive) of visible view indexes. */
    public @Nullable Pair<Integer, Integer> getVisibleRange() {
        return mGetVisibleIndex == null ? null : mGetVisibleIndex.get();
    }

    /** Returns the root view at a given index. */
    public @Nullable View getViewByIndex(int viewIndex) {
        return mFetchViewByIndex == null ? null : mFetchViewByIndex.apply(viewIndex);
    }

    /** Returns a nested supplier for the scrolling state of the view. */
    public OneshotSupplier<ObservableSupplier<Boolean>> getIsScrollingSupplier() {
        return mIsScrollingSupplier;
    }

    /**
     * Sets the content sensitivity on the recycler view of the tab switcher.
     *
     * @param contentIsSensitive True if the tab switcher is sensitive.
     */
    public void setTabSwitcherContentSensitivity(boolean contentIsSensitive) {
        mContainerViewModel.set(
                TabListContainerProperties.IS_CONTENT_SENSITIVE, contentIsSensitive);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    @VisibleForTesting
    CancelLongPressTabItemEventListener onLongPressOnTabCard(
            TabGridContextMenuCoordinator tabGridContextMenuCoordinator,
            TabListGroupMenuCoordinator tabListGroupMenuCoordinator,
            @TabId int tabId,
            @Nullable View cardView) {
        TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
        @Nullable Tab tab = filter.getTabModel().getTabById(tabId);
        if (tab == null
                || cardView == null
                || !ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()) {
            return null;
        }

        ViewRectProvider viewRectProvider =
                new ViewRectProvider(cardView, TabGridViewRectUpdater::new);
        Token groupId = tab.getTabGroupId();
        if (groupId != null) {
            tabListGroupMenuCoordinator.showMenu(viewRectProvider, groupId);
            return tabListGroupMenuCoordinator::dismiss;
        } else {
            tabGridContextMenuCoordinator.showMenu(viewRectProvider, tabId);
            RecordUserAction.record("TabSwitcher.ContextMenu");
            return tabGridContextMenuCoordinator::dismiss;
        }
    }

    private boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator == null ? false : mTabGridDialogCoordinator.isVisible();
    }

    private void onTabSwitcherShown() {
        mTabListCoordinator.attachEmptyView();
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        // Returning null causes the animation to be a fade.
        // Do so if we are animating to show or hide the HubLayout or this is a low end device.
        if (mIsAnimatingSupplier.get() || SysUtils.isLowEndDevice()) return null;

        TabListCoordinator coordinator = mTabListCoordinator;
        int index = coordinator.getTabIndexFromTabId(tabId);
        ViewHolder sourceViewHolder =
                coordinator.getContainerView().findViewHolderForAdapterPosition(index);
        // TODO(crbug.com/41479135): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        return sourceViewHolder == null ? null : sourceViewHolder.itemView;
    }

    private void onVisibilityChanged(boolean visible) {
        if (visible) {
            mMessageManager.bind(
                    mTabListCoordinator,
                    mParentView,
                    /* priceWelcomeMessageReviewActionProvider= */ mMediator,
                    (tabId) -> mMediator.onTabSelecting(tabId, false));
            mMessageManager.addObserver(mMessageUpdateObserver);
            updateBottomPadding();
            mTabListCoordinator.prepareTabSwitcherPaneView();
        } else {
            mMessageManager.removeObserver(mMessageUpdateObserver);
            mMessageManager.unbind(mTabListCoordinator);
            updateBottomPadding();
            mTabListCoordinator.postHiding();
        }
    }

    private GridCardOnClickListenerProvider getGridCardOnClickListenerProvider() {
        return mMediator;
    }

    private PriceWelcomeMessageController getPriceWelcomeMessageController() {
        return mMessageManager;
    }

    private CancelLongPressTabItemEventListener onLongPressOnTabCard(
            int tabId, @Nullable View cardView) {
        return onLongPressOnTabCard(
                mContextMenuCoordinator,
                mTabListCoordinator.getTabListGroupMenuCoordinator(),
                tabId,
                cardView);
    }

    private void onEdgeToEdgeControllerChanged(
            @Nullable EdgeToEdgeController newController,
            @Nullable EdgeToEdgeController oldController) {
        if (oldController != null) {
            oldController.unregisterAdjuster(mEdgeToEdgePadAdjuster);
        }
        if (newController != null && mEdgeToEdgePadAdjuster != null) {
            newController.registerAdjuster(mEdgeToEdgePadAdjuster);
        }
    }

    /** Returns the container view property model for testing. */
    @NonNull
    PropertyModel getContainerViewModelForTesting() {
        return mContainerViewModel;
    }

    /** Returns the dialog controller for testing. */
    @NonNull
    DialogController getTabGridDialogControllerForTesting() {
        return mDialogControllerSupplier.get();
    }

    /** Return the Edge to edge pad adjuster. */
    @Nullable
    EdgeToEdgePadAdjuster getEdgeToEdgePadAdjusterForTesting() {
        return mEdgeToEdgePadAdjuster;
    }

    /* package */ TabGridDialogCoordinator getTabGridDialogCoordinatorForTesting() {
        return mTabGridDialogCoordinator;
    }

    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        mTabListCoordinator.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    /** Returns the filter index of a tab from its view index. */
    public int countOfTabCardsOrInvalid(int viewIndex) {
        return mTabListCoordinator.indexOfTabCardsOrInvalid(viewIndex);
    }

    private int getNthTabIndexInModel(int filterIndex) {
        assert mTabListCoordinator != null;
        int indexInModel = mTabListCoordinator.getIndexOfNthTabCard(filterIndex);
        // If the tab list coordinator doesn't contain tab data yet assume filterIndex is a
        // sufficient approximation as the offset would be caused by message cards.
        if (indexInModel == TabList.INVALID_TAB_INDEX) return filterIndex;

        return indexInModel;
    }

    private void updateBottomPadding() {
        int bottomPadding = 0;
        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            bottomPadding = mEdgeToEdgeBottomInsets;
            mContainerViewModel.set(
                    TabListContainerProperties.IS_CLIP_TO_PADDING, bottomPadding == 0);
        }
        mContainerViewModel.set(TabListContainerProperties.BOTTOM_PADDING, bottomPadding);
    }

    private void onFilterChange(TabGroupModelFilter filter) {
        if (mTabGroupListBottomSheetCoordinator != null) {
            mTabGroupListBottomSheetCoordinator.destroy();
        }

        TabGroupCreationDialogManager tabGroupCreationDialogManager =
                new TabGroupCreationDialogManager(
                        mActivity, mModalDialogManager, mOnTabGroupCreation);

        Profile profile = filter.getTabModel().getProfile();
        if (profile == null) return;

        mTabGroupListBottomSheetCoordinator =
                new TabGroupListBottomSheetCoordinator(
                        mActivity,
                        profile,
                        tabGroupId -> tabGroupCreationDialogManager.showDialog(tabGroupId, filter),
                        /* tabMovedCallback= */ null,
                        filter,
                        mBottomSheetController,
                        /* supportsShowNewGroup= */ true,
                        /* destroyOnHide= */ false);

        ShowTabListEditor showTabListEditor =
                tabId -> {
                    mTabListEditorManager.showTabListEditor();
                    TabListEditorController tabListEditorController =
                            mTabListEditorManager.getControllerSupplier().get();
                    if (tabListEditorController != null) {
                        tabListEditorController.selectTabs(
                                Set.of(TabListEditorItemSelectionId.createTabId(tabId)));
                    }
                };
        mContextMenuCoordinator =
                TabGridContextMenuCoordinator.createContextMenuCoordinator(
                        mActivity,
                        mTabBookmarkerSupplier,
                        filter,
                        mTabGroupListBottomSheetCoordinator,
                        tabGroupCreationDialogManager,
                        mShareDelegateSupplier,
                        showTabListEditor);
    }
}
