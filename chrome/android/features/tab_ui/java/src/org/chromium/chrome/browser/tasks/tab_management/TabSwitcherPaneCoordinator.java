// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BROWSER_CONTROLS_STATE_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.FETCH_VIEW_BY_INDEX_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.GET_VISIBLE_RANGE_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_SCROLLING_SUPPLIER_CALLBACK;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.MODE;

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
import androidx.core.util.Function;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.TraceEvent;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.invitation_dialog.DataSharingInvitationDialogCoordinator;
import org.chromium.chrome.browser.hub.HubFieldTrial;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

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

    private final TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener
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
    private final Runnable mOnDestroyed;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final int mFloatingButtonSpace;
    private final TabListOnScrollListener mTabListOnScrollListener = new TabListOnScrollListener();
    private final OneshotSupplierImpl<ObservableSupplier<Boolean>> mIsScrollingSupplier =
            new OneshotSupplierImpl<>();
    private final Callback<EdgeToEdgeController> mOnEdgeToEdgeControllerChangedCallback =
            new ValueChangedCallback<>(this::onEdgeToEdgeControllerChanged);

    /** Lazily initialized when shown. */
    private @Nullable TabGridDialogCoordinator mTabGridDialogCoordinator;

    private @Nullable DataSharingInvitationDialogCoordinator mDataSharingDialogCoordinator;
    private @Nullable Function<Integer, View> mFetchViewByIndex;
    private @Nullable Supplier<Pair<Integer, Integer>> mGetVisibleIndex;

    /** Not null when drawing the hub edge to edge. */
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    private int mEdgeToEdgeBottomInsets;

    /**
     * @param activity The {@link Activity} that hosts the pane.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabModelFilterSupplier The supplier of the tab model filter fo rthis pane.
     * @param tabContentManager For management of thumbnails.
     * @param tabCreatorManager For creating new tabs.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param scrimCoordinator The scrim coordinator to use for the tab grid dialog.
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
     * @param desktopWindowStateProvider Provider to get desktop window and app header state.
     */
    public TabSwitcherPaneCoordinator(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull ObservableSupplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
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
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        try (TraceEvent e = TraceEvent.scoped("TabSwitcherPaneCoordinator.constructor")) {
            mProfileProviderSupplier = profileProviderSupplier;
            mIsVisibleSupplier = isVisibleSupplier;
            mIsAnimatingSupplier = isAnimatingSupplier;
            mActivity = activity;
            mModalDialogManager = modalDialogManager;
            mParentView = parentView;
            mOnDestroyed = onDestroyed;
            mEdgeToEdgeSupplier = edgeToEdgeSupplier;
            mFloatingButtonSpace =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.floating_action_button_space);

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
                            .build();

            mContainerViewModel = containerViewModel;

            TabGroupModelFilter filter = (TabGroupModelFilter) tabModelFilterSupplier.get();
            Profile profile = mProfileProviderSupplier.get().getOriginalProfile();
            ActionConfirmationManager actionConfirmationManager =
                    filter.isIncognitoBranded()
                            ? null
                            : new ActionConfirmationManager(
                                    profile, mActivity, filter, mModalDialogManager);

            mDialogControllerSupplier =
                    LazyOneshotSupplier.fromSupplier(
                            () -> {
                                mTabGridDialogCoordinator =
                                        new TabGridDialogCoordinator(
                                                activity,
                                                browserControlsStateProvider,
                                                bottomSheetController,
                                                dataSharingTabManager,
                                                tabModelFilterSupplier,
                                                tabContentManager,
                                                tabCreatorManager,
                                                coordinatorView,
                                                resetHandler,
                                                getGridCardOnClickListenerProvider(),
                                                TabSwitcherPaneCoordinator.this
                                                        ::getTabGridDialogAnimationSourceView,
                                                scrimCoordinator,
                                                getTabGroupTitleEditor(),
                                                actionConfirmationManager,
                                                mModalDialogManager,
                                                desktopWindowStateProvider);
                                return mTabGridDialogCoordinator.getDialogController();
                            });

            mMediator =
                    new TabSwitcherPaneMediator(
                            resetHandler,
                            tabModelFilterSupplier,
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
                            tabModelFilterSupplier);

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
                            tabModelFilterSupplier,
                            mMultiThumbnailCardProvider,
                            /* actionOnRelatedTabs= */ true,
                            actionConfirmationManager,
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
                            tabModelFilterSupplier,
                            tabContentManager,
                            tabListCoordinator,
                            bottomSheetController,
                            mode,
                            onTabGroupCreation,
                            desktopWindowStateProvider,
                            mEdgeToEdgeSupplier);
            mTabListEditorManager = tabListEditorManager;
            mMediator.setTabListEditorControllerSupplier(
                    mTabListEditorManager.getControllerSupplier());

            mMessageManager = messageManager;
            mMessageManager.registerMessages(tabListCoordinator);

            mOnVisibilityChanged.onResult(isVisibleSupplier.addObserver(mOnVisibilityChanged));
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
    public void resetWithTabList(@Nullable TabList tabList) {
        List<Tab> tabs = TabModelUtils.convertTabListToListOfTabs(tabList);
        mMessageManager.beforeReset();
        // Quick mode being false here ensures the selected tab's thumbnail gets updated. With Hub
        // the TabListCoordinator no longer triggers thumbnail captures so this shouldn't guard
        // against the large amount of work that is used to.
        mTabListCoordinator.resetWithListOfTabs(
                tabList == null ? null : tabs, /* quickMode= */ false);
        mMessageManager.afterReset(tabs.size());
        mTabListOnScrollListener.postUpdate(mTabListCoordinator.getContainerView());
    }

    /** Performs soft cleanup which removes thumbnails to relieve memory usage. */
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    /** Performs hard cleanup which saves price drop information. */
    public void hardCleanup() {
        mTabListCoordinator.hardCleanup();
        // TODO(crbug.com/40946413): The pre-fork implementation resets the tab list, this seems
        // suboptimal. Consider not doing this.
        resetWithTabList(null);
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
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogVisibilitySupplier;
    }

    /** Returns a {@link TabSwitcherCustomViewManager.Delegate} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager.Delegate
            getTabSwitcherCustomViewManagerDelegate() {
        return mMediator;
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

    @Override
    public @BackPressResult int handleBackPress() {
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }

    private boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator == null ? false : mTabGridDialogCoordinator.isVisible();
    }

    private void onTabSwitcherShown() {
        mTabListCoordinator.attachEmptyView();
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        // If we are animating to show or hide the HubLayout, the TabGridDialog should hide or show
        // via fade instead of animating from a tab. Return null so that this happens.
        if (mIsAnimatingSupplier.get()) return null;

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

    private TabGroupTitleEditor getTabGroupTitleEditor() {
        return mTabListCoordinator.getTabGroupTitleEditor();
    }

    private PriceWelcomeMessageController getPriceWelcomeMessageController() {
        return mMessageManager;
    }

    private void onLongPressOnTabCard(int tabId) {
        mTabListEditorManager.showTabListEditor();
        RecordUserAction.record("TabMultiSelectV2.OpenLongPressInGrid");
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
        if (HubFieldTrial.usesFloatActionButton() && mTabListCoordinator.isLastItemMessage()) {
            bottomPadding += mFloatingButtonSpace;
        }
        mContainerViewModel.set(TabListContainerProperties.BOTTOM_PADDING, bottomPadding);
    }

    /**
     * Open the invitation modal on top of the tab switcher view when an invitation intent is
     * intercepted.
     *
     * @param invitationId The id of the invitation.
     */
    public void openInvitationModal(String invitationId) {
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.hideDialog(true);
        }

        if (mDataSharingDialogCoordinator == null) {
            mDataSharingDialogCoordinator =
                    new DataSharingInvitationDialogCoordinator(mActivity, mModalDialogManager);
        }
        mDataSharingDialogCoordinator.show();
    }
}
