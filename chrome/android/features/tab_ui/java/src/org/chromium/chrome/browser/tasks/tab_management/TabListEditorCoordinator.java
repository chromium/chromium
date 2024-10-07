// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.GridCardOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * This class is a coordinator for TabListEditor component. It manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component.
 */
class TabListEditorCoordinator {
    static final String COMPONENT_NAME = "TabListEditor";

    // TODO(crbug.com/41467140): Unify similar interfaces in other components that used the
    // TabListCoordinator.
    /** Interface for resetting the selectable tab grid. */
    interface ResetHandler {
        /**
         * Handles the reset event.
         *
         * @param tabs List of {@link Tab}s to reset.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         * @param quickMode whether to use quick mode.
         */
        void resetWithListOfTabs(
                @Nullable List<Tab> tabs,
                @Nullable RecyclerViewPosition recyclerViewPosition,
                boolean quickMode);

        /** Handles syncing the position of the outer {@link TabListCoordinator}'s RecyclerView. */
        void syncRecyclerViewPosition();

        /** Handles cleanup. */
        void postHiding();
    }

    /** An interface to control the TabListEditor. */
    interface TabListEditorController extends BackPressHandler {
        /**
         * Shows the TabListEditor with the given {@Link Tab}s.
         *
         * @param tabs List of {@link Tab}s to show.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         */
        void show(List<Tab> tabs, @Nullable RecyclerViewPosition recyclerViewPosition);

        /** Hides the TabListEditor. */
        void hide();

        /**
         * @return Whether or not the TabListEditor consumed the event.
         */
        boolean handleBackPressed();

        /**
         * Configure the Toolbar for TabListEditor with multiple actions.
         *
         * @param actions The {@link TabListEditorAction} to make available.
         */
        void configureToolbarWithMenuItems(List<TabListEditorAction> actions);

        /**
         * @return Whether the TabListEditor is visible.
         */
        boolean isVisible();

        /** Sets the toolbar title when no items are selected. */
        void setToolbarTitle(String title);

        /** Sets a custom {@link NavigationProvider} to handle "back" actions. */
        void setNavigationProvider(@NonNull NavigationProvider navigationProvider);

        /** Sets the {@link TabActionState} for the TabListEditor. */
        void setTabActionState(@TabActionState int tabActionState);

        /** Sets the {@link LifecycleObserver} for this TabListEditor. */
        void setLifecycleObserver(LifecycleObserver lifecycleObserver);
    }

    /** An interface for embedders to provide navigation. */
    public interface NavigationProvider {
        /** Defines what to do to handle "back" actions. */
        void goBack();
    }

    /** Allows an embedder to observe the lifecycle of the TabListEditor. */
    public interface LifecycleObserver {
        /** Called when the TabListEditor is about to be hidden. */
        void willHide();

        /** Called after the TabListEditor is hidden. */
        void didHide();
    }

    /** Provider of action for the navigation button in {@link TabListEditorMediator}. */
    public static class TabListEditorNavigationProvider implements NavigationProvider {
        private final TabListEditorCoordinator.TabListEditorController mTabListEditorController;
        private final Context mContext;

        public TabListEditorNavigationProvider(
                Context context,
                TabListEditorCoordinator.TabListEditorController tabListEditorController) {
            mContext = context;
            mTabListEditorController = tabListEditorController;
        }

        @Override
        public void goBack() {
            TabUiMetricsHelper.recordSelectionEditorExitMetrics(
                    TabListEditorExitMetricGroups.CLOSED_BY_USER, mContext);
            mTabListEditorController.hide();
        }
    }

    private final TabListEditorController mTabListEditorController =
            new TabListEditorController() {
                @Override
                public void show(
                        List<Tab> tabs, @Nullable RecyclerViewPosition recyclerViewPosition) {
                    if (mTabListCoordinator == null) {
                        createTabListCoordinator();
                    }
                    mTabListEditorMediator.show(tabs, recyclerViewPosition);
                }

                @Override
                public void hide() {
                    mTabListEditorMediator.hide();
                }

                @Override
                public void configureToolbarWithMenuItems(List<TabListEditorAction> actions) {
                    assert mTabListCoordinator != null
                            : "Must call #show before #configureToolbarWithMenuItems";
                    mTabListEditorMediator.configureToolbarWithMenuItems(actions);
                }

                @Override
                public boolean isVisible() {
                    return mTabListEditorMediator.isVisible();
                }

                @Override
                public void setToolbarTitle(String title) {
                    mTabListEditorMediator.setToolbarTitle(title);
                }

                @Override
                public void setNavigationProvider(NavigationProvider navigationProvider) {
                    mTabListEditorMediator.setNavigationProvider(navigationProvider);
                }

                @Override
                public boolean handleBackPressed() {
                    return mTabListEditorMediator.handleBackPressed();
                }

                @Override
                public @BackPressResult int handleBackPress() {
                    return mTabListEditorMediator.handleBackPress();
                }

                @Override
                public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
                    return mTabListEditorMediator.getHandleBackPressChangedSupplier();
                }

                @Override
                public void setTabActionState(@TabActionState int tabActionState) {
                    mTabActionState = tabActionState;
                    mTabListEditorMediator.setTabActionState(tabActionState);
                }

                @Override
                public void setLifecycleObserver(LifecycleObserver lifecycleObserver) {
                    mTabListEditorMediator.setLifecycleObserver(lifecycleObserver);
                }
            };

    private final Context mContext;
    private final ViewGroup mRootView;
    private final ViewGroup mParentView;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final TabListEditorLayout mTabListEditorLayout;
    private final SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();
    private final PropertyModel mModel;
    private final TabListEditorMediator mTabListEditorMediator;
    private final Callback<RecyclerViewPosition> mClientTabListRecyclerViewPositionSetter;

    private final @TabListMode int mTabListMode;
    private final boolean mDisplayGroups;
    private final TabContentManager mTabContentManager;
    private final @Nullable GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @Nullable ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;

    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private TabListCoordinator mTabListCoordinator;
    private PropertyModelChangeProcessor mTabListEditorLayoutChangeProcessor;
    private @TabActionState int mTabActionState;
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /**
     * @param context The Android context to use.
     * @param rootView The top ViewGroup which has parentView attached to it, or the same if no
     *     custom parentView is present.
     * @param parentView The ViewGroup which the TabListEditor will attach itself to it may be
     *     rootView if no custom view is being used, or a sub-view which is then attached to
     *     rootView.
     * @param browserControlsStateProvider Provides the browser controls state.
     * @param currentTabModelFilterSupplier Supplies the current TabModelFilter.
     * @param tabContentManager Provides thumbnails for tabs.
     * @param clientTabListRecyclerViewPositionSetter Allows setting the recycler view position.
     * @param mode Modes of showing the list of tabs. Can be used in GRID or STRIP.
     * @param displayGroups Whether groups should be displayed.
     * @param snackbarManager Used to display snackbar messages.
     * @param bottomSheetController Used to display bottom sheets.
     * @param initialTabActionState The initial TabActionState to use.
     * @param modalDialogManager Used for managing the modal dialogs.
     * @param desktopWindowStateProvider Provider to get desktop window and app header state.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    public TabListEditorCoordinator(
            Context context,
            ViewGroup rootView,
            ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            TabContentManager tabContentManager,
            Callback<RecyclerViewPosition> clientTabListRecyclerViewPositionSetter,
            @TabListMode int mode,
            boolean displayGroups,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            @TabActionState int initialTabActionState,
            @Nullable GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @NonNull ModalDialogManager modalDialogManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider,
            @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        try (TraceEvent e = TraceEvent.scoped("TabListEditorCoordinator.constructor")) {
            mContext = context;
            mRootView = rootView;
            mParentView = parentView;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
            mClientTabListRecyclerViewPositionSetter = clientTabListRecyclerViewPositionSetter;
            mTabListMode = mode;
            mDisplayGroups = displayGroups;
            mTabActionState = initialTabActionState;
            mTabContentManager = tabContentManager;
            assert mode == TabListCoordinator.TabListMode.GRID
                    || mode == TabListCoordinator.TabListMode.LIST;
            mGridCardOnClickListenerProvider = gridCardOnClickListenerProvider;
            mModalDialogManager = modalDialogManager;
            mEdgeToEdgeSupplier = edgeToEdgeSupplier;

            // The change processor isn't created until TabListCoordinator is created (lazily).
            mTabListEditorLayout =
                    LayoutInflater.from(context)
                            .inflate(R.layout.tab_list_editor_layout, parentView, false)
                            .findViewById(R.id.selectable_list);
            mModel = new PropertyModel.Builder(TabListEditorProperties.ALL_KEYS).build();

            // TODO(crbug.com/40881091): Refactor SnackbarManager to support multiple overridden
            // parentViews in a stack to avoid contention and using new snackbar managers.
            mTabListEditorMediator =
                    new TabListEditorMediator(
                            mContext,
                            mCurrentTabModelFilterSupplier,
                            mModel,
                            mSelectionDelegate,
                            displayGroups,
                            snackbarManager,
                            bottomSheetController,
                            mTabListEditorLayout,
                            mTabActionState,
                            desktopWindowStateProvider);
            mTabListEditorMediator.setNavigationProvider(
                    new TabListEditorNavigationProvider(mContext, mTabListEditorController));
        }
    }

    /**
     * @return The {@link SelectionDelegate} that is used in this component.
     */
    SelectionDelegate<Integer> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    /**
     * Resets {@link TabListCoordinator} with the provided list.
     *
     * @param tabs List of {@link Tab}s to reset.
     * @param quickMode whether to use quick mode.
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs, boolean quickMode) {
        mTabListCoordinator.resetWithListOfTabs(tabs, quickMode);
    }

    /**
     * @return {@link TabListEditorController} that can control the TabListEditor.
     */
    TabListEditorController getController() {
        return mTabListEditorController;
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        if (mTabListCoordinator != null) {
            mTabListCoordinator.onDestroy();
            mTabListCoordinator = null;
        }
        if (mTabListEditorLayoutChangeProcessor != null) {
            mTabListEditorLayoutChangeProcessor.destroy();
            mTabListEditorLayoutChangeProcessor = null;
        }

        mTabListEditorLayout.destroy();
        mTabListEditorMediator.destroy();
        if (mMultiThumbnailCardProvider != null) {
            mMultiThumbnailCardProvider.destroy();
        }

        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }
    }

    /**
     * Register a new view type for the underlying TabListCoordinator.
     *
     * @see MVCListAdapter#registerType(int, MVCListAdapter.ViewBuilder,
     *     PropertyModelChangeProcessor.ViewBinder).
     */
    public <T extends View> void registerItemType(
            @UiType int typeId,
            MVCListAdapter.ViewBuilder<T> builder,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mTabListCoordinator != null;
        mTabListCoordinator.registerItemType(typeId, builder, binder);
    }

    /**
     * Inserts a special item into the underlying TabListCoordinator.
     *
     * @see TabListCoordinator#addSpecialItemToModel(int, int, PropertyModel).
     */
    public void addSpecialListItem(int index, @UiType int uiType, PropertyModel model) {
        assert mTabListCoordinator != null;
        mTabListCoordinator.addSpecialListItem(index, uiType, model);
    }

    /**
     * Removes a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} that has the
     * given {@code uiType} and/or its {@link PropertyModel} has the given {@code itemIdentifier}.
     *
     * @param uiType The uiType to match.
     * @param itemIdentifier The itemIdentifier to match. This can be obsoleted if the {@link
     *     org.chromium.ui.modelutil.MVCListAdapter.ListItem} does not need additional identifier.
     */
    public void removeSpecialListItem(@UiType int uiType, int itemIdentifier) {
        assert mTabListCoordinator != null;
        mTabListCoordinator.removeSpecialListItem(uiType, itemIdentifier);
    }

    /**
     * Override the content descriptions of the top-level layout and back button.
     *
     * @param containerContentDescription The content description for the top-level layout.
     * @param backButtonContentDescription The content description for the back button.
     */
    public void overrideContentDescriptions(
            @StringRes int containerContentDescription,
            @StringRes int backButtonContentDescription) {
        mTabListEditorLayout.overrideContentDescriptions(
                containerContentDescription, backButtonContentDescription);
    }

    private void createTabListCoordinator() {
        Profile regularProfile =
                mCurrentTabModelFilterSupplier
                        .get()
                        .getTabModel()
                        .getProfile()
                        .getOriginalProfile();

        ResetHandler resetHandler =
                new ResetHandler() {
                    @Override
                    public void resetWithListOfTabs(
                            @Nullable List<Tab> tabs,
                            @Nullable RecyclerViewPosition recyclerViewPosition,
                            boolean quickMode) {
                        TabListEditorCoordinator.this.resetWithListOfTabs(tabs, quickMode);
                        if (recyclerViewPosition == null) {
                            return;
                        }

                        mTabListCoordinator.setRecyclerViewPosition(recyclerViewPosition);
                    }

                    @Override
                    public void syncRecyclerViewPosition() {
                        if (mClientTabListRecyclerViewPositionSetter == null) {
                            return;
                        }

                        mClientTabListRecyclerViewPositionSetter.onResult(
                                mTabListCoordinator.getRecyclerViewPosition());
                    }

                    @Override
                    public void postHiding() {
                        mTabListCoordinator.postHiding();
                        mTabListCoordinator.softCleanup();
                        mTabListCoordinator.resetWithListOfTabs(null, /* quickMode= */ false);
                    }
                };

        ThumbnailProvider thumbnailProvider =
                initMultiThumbnailCardProvider(mDisplayGroups, mTabContentManager);
        if (mMultiThumbnailCardProvider != null) {
            mMultiThumbnailCardProvider.initWithNative(regularProfile);
        }
        mTabListCoordinator =
                new TabListCoordinator(
                        mTabListMode,
                        mContext,
                        mBrowserControlsStateProvider,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        thumbnailProvider,
                        mDisplayGroups,
                        /* actionConfirmationManager= */ null,
                        mGridCardOnClickListenerProvider,
                        /* dialogHandler= */ null,
                        mTabActionState,
                        this::getSelectionDelegate,
                        /* priceWelcomeMessageControllerSupplier= */ null,
                        mTabListEditorLayout,
                        /* attachToParent= */ false,
                        COMPONENT_NAME,
                        /* onModelTokenChange= */ null,
                        /* hasEmptyView= */ false,
                        /* emptyImageResId= */ Resources.ID_NULL,
                        /* emptyHeadingStringResId= */ Resources.ID_NULL,
                        /* emptySubheadingStringResId= */ Resources.ID_NULL,
                        /* onTabGroupCreation= */ null,
                        /* allowDragAndDrop= */ false);

        // Note: The TabListEditorCoordinator is always created after native is initialized.
        mTabListCoordinator.initWithNative(regularProfile);

        RecyclerView.LayoutManager layoutManager =
                mTabListCoordinator.getContainerView().getLayoutManager();
        if (layoutManager instanceof GridLayoutManager) {
            ((GridLayoutManager) layoutManager)
                    .setSpanSizeLookup(
                            new GridLayoutManager.SpanSizeLookup() {
                                @Override
                                public int getSpanSize(int i) {
                                    return 1;
                                }
                            });
        }

        mTabListEditorLayout.initialize(
                mRootView,
                mParentView,
                mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(),
                mSelectionDelegate);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);
        mTabListEditorMediator.initializeWithTabListCoordinator(mTabListCoordinator, resetHandler);

        mTabListEditorLayoutChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mTabListEditorLayout, TabListEditorLayoutBinder::bind);

        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()
                && mEdgeToEdgeSupplier != null
                && mDisplayGroups) {
            assert mTabListMode != TabListMode.STRIP
                    : "STRIP tab lists should not be padded for edge-to-edge.";
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            mTabListCoordinator.getContainerView(), mEdgeToEdgeSupplier);
        }
    }

    public void addTabListItemSizeChangedObserver(TabListItemSizeChangedObserver observer) {
        assert mTabListCoordinator != null;
        mTabListCoordinator.addTabListItemSizeChangedObserver(observer);
    }

    public void removeTabListItemSizeChangedObserver(TabListItemSizeChangedObserver observer) {
        assert mTabListCoordinator != null;
        mTabListCoordinator.removeTabListItemSizeChangedObserver(observer);
    }

    private ThumbnailProvider initMultiThumbnailCardProvider(
            boolean displayGroups, TabContentManager tabContentManager) {
        if (displayGroups) {
            mMultiThumbnailCardProvider =
                    new MultiThumbnailCardProvider(
                            mContext,
                            mBrowserControlsStateProvider,
                            tabContentManager,
                            mCurrentTabModelFilterSupplier);
            return mMultiThumbnailCardProvider;
        }
        return new TabContentManagerThumbnailProvider(tabContentManager);
    }

    // Testing-specific methods

    /** Returns the {@link TabListEditorLayout} for testing. */
    TabListEditorLayout getTabListEditorLayoutForTesting() {
        return mTabListEditorLayout;
    }

    /** Returns the {@link TabListRecyclerView} for testing. */
    TabListRecyclerView getTabListRecyclerViewForTesting() {
        return mTabListCoordinator.getContainerView();
    }

    /** Returns the {@link EdgeToEdgePadAdjuster} for testing. */
    EdgeToEdgePadAdjuster getEdgeToEdgePadAdjusterForTesting() {
        return mEdgeToEdgePadAdjuster;
    }
}
