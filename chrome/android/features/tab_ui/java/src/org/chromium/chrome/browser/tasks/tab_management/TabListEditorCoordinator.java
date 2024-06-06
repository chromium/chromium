// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.LayoutViewBuilder;
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
         * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         * @param quickMode whether to use quick mode.
         */
        void resetWithListOfTabs(
                @Nullable List<Tab> tabs,
                int preSelectedCount,
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
         * Shows the TabListEditor with the given {@Link Tab}s, and the first
         * {@code preSelectedTabCount} tabs being selected.
         * @param tabs List of {@link Tab}s to show.
         * @param preSelectedTabCount Number of selected {@link Tab}s.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         */
        void show(
                List<Tab> tabs,
                int preSelectedTabCount,
                @Nullable RecyclerViewPosition recyclerViewPosition);

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
    }

    /** An interface for embedders to provide navigation. */
    public interface NavigationProvider {
        /** Defines what to do to handle "back" actions. */
        void goBack();
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
                        List<Tab> tabs,
                        int preSelectedTabCount,
                        @Nullable RecyclerViewPosition recyclerViewPosition) {
                    if (mTabListCoordinator == null) {
                        createTabListCoordinator();
                    }
                    mTabListEditorMediator.show(tabs, preSelectedTabCount, recyclerViewPosition);
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
            };

    private final Context mContext;
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
    private final @TabActionState int mInitialTabActionState;
    private final ViewGroup mRootView;
    private final TabContentManager mTabContentManager;

    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private TabListCoordinator mTabListCoordinator;
    private PropertyModelChangeProcessor mTabListEditorLayoutChangeProcessor;

    public TabListEditorCoordinator(
            Context context,
            ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            TabContentManager tabContentManager,
            Callback<RecyclerViewPosition> clientTabListRecyclerViewPositionSetter,
            @TabListMode int mode,
            ViewGroup rootView,
            boolean displayGroups,
            SnackbarManager snackbarManager,
            @TabActionState int initialTabActionState) {
        try (TraceEvent e = TraceEvent.scoped("TabListEditorCoordinator.constructor")) {
            mContext = context;
            mParentView = parentView;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
            mClientTabListRecyclerViewPositionSetter = clientTabListRecyclerViewPositionSetter;
            mTabListMode = mode;
            mDisplayGroups = displayGroups;
            mInitialTabActionState = initialTabActionState;
            mRootView = rootView;
            mTabContentManager = tabContentManager;
            assert mode == TabListCoordinator.TabListMode.GRID
                    || mode == TabListCoordinator.TabListMode.LIST;

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
                            mTabListEditorLayout,
                            initialTabActionState);
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
     * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
     * @param quickMode whether to use quick mode.
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount, boolean quickMode) {
        mTabListCoordinator.resetWithListOfTabs(tabs, quickMode);

        if (tabs != null && preSelectedCount > 0 && preSelectedCount < tabs.size()) {
            mTabListCoordinator.addSpecialListItem(
                    preSelectedCount,
                    TabProperties.UiType.DIVIDER,
                    new PropertyModel.Builder(CARD_TYPE).with(CARD_TYPE, OTHERS).build());
        }
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
                            int preSelectedCount,
                            @Nullable RecyclerViewPosition recyclerViewPosition,
                            boolean quickMode) {
                        TabListEditorCoordinator.this.resetWithListOfTabs(
                                tabs, preSelectedCount, quickMode);
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
                        mCurrentTabModelFilterSupplier,
                        thumbnailProvider,
                        mDisplayGroups,
                        null,
                        null,
                        mInitialTabActionState,
                        this::getSelectionDelegate,
                        null,
                        mTabListEditorLayout,
                        false,
                        COMPONENT_NAME,
                        mRootView,
                        null);

        // Note: The TabListEditorCoordinator is always created after native is
        // initialized.
        mTabListCoordinator.initWithNative(regularProfile, null);

        mTabListCoordinator.registerItemType(
                TabProperties.UiType.DIVIDER,
                new LayoutViewBuilder(R.layout.horizontal_divider),
                (model, view, propertyKey) -> {});
        RecyclerView.LayoutManager layoutManager =
                mTabListCoordinator.getContainerView().getLayoutManager();
        if (layoutManager instanceof GridLayoutManager) {
            ((GridLayoutManager) layoutManager)
                    .setSpanSizeLookup(
                            new GridLayoutManager.SpanSizeLookup() {
                                @Override
                                public int getSpanSize(int i) {
                                    int itemType =
                                            mTabListCoordinator
                                                    .getContainerView()
                                                    .getAdapter()
                                                    .getItemViewType(i);

                                    if (itemType == TabProperties.UiType.DIVIDER) {
                                        return ((GridLayoutManager) layoutManager).getSpanCount();
                                    }
                                    return 1;
                                }
                            });
        }

        mTabListEditorLayout.initialize(
                mParentView,
                mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(),
                mSelectionDelegate);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);
        mTabListEditorMediator.initializeWithTabListCoordinator(mTabListCoordinator, resetHandler);

        mTabListEditorLayoutChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mTabListEditorLayout, TabListEditorLayoutBinder::bind);
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
        return (tabId, thumbnailSize, callback, forceUpdate, writeBack, isSelected) -> {
            tabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeBack);
        };
    }

    // Testing-specific methods

    /**
     * @return The {@link TabListEditorLayout} for testing.
     */
    TabListEditorLayout getTabListEditorLayoutForTesting() {
        return mTabListEditorLayout;
    }

    /**
     * @return The {@link TabListRecyclerView} for testing.
     */
    TabListRecyclerView getTabListRecyclerViewForTesting() {
        return mTabListCoordinator.getContainerView();
    }
}
