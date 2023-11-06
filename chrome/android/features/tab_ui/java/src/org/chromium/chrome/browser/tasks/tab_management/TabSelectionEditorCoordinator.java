// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;
import static org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorProperties.IS_VISIBLE;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabSelectionEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * This class is a coordinator for TabSelectionEditor component. It manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component.
 */
class TabSelectionEditorCoordinator {
    static final String COMPONENT_NAME = "TabSelectionEditor";

    // TODO(977271): Unify similar interfaces in other components that used the TabListCoordinator.
    /**
     * Interface for resetting the selectable tab grid.
     */
    interface ResetHandler {
        /**
         * Handles the reset event.
         * @param tabs List of {@link Tab}s to reset.
         * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         * @param quickMode whether to use quick mode.
         */
        void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount,
                @Nullable RecyclerViewPosition recyclerViewPosition, boolean quickMode);

        /**
         * Handles syncing the position of the outer {@link TabListCoordinator}'s RecyclerView.
         */
        void syncRecyclerViewPosition();

        /**
         * Handles cleanup.
         */
        void postHiding();
    }

    /**
     * An interface to control the TabSelectionEditor.
     */
    interface TabSelectionEditorController extends BackPressHandler {
        /**
         * Shows the TabSelectionEditor with the given {@Link Tab}s, and the first
         * {@code preSelectedTabCount} tabs being selected.
         * @param tabs List of {@link Tab}s to show.
         * @param preSelectedTabCount Number of selected {@link Tab}s.
         * @param recyclerViewPosition The state to preserve scroll position of the recycler view.
         */
        void show(List<Tab> tabs, int preSelectedTabCount,
                @Nullable RecyclerViewPosition recyclerViewPosition);

        /**
         * Hides the TabSelectionEditor.
         */
        void hide();

        /**
         * @return Whether or not the TabSelectionEditor consumed the event.
         */
        boolean handleBackPressed();

        /**
         * Configure the Toolbar for TabSelectionEditor with multiple actions.
         * @param actions The {@link TabSelectionEditorAction} to make available.
         * @param navigationProvider The {@link TabSelectionEditorNavigationProvider} that specifies
         *         the back action.
         */
        void configureToolbarWithMenuItems(List<TabSelectionEditorAction> actions,
                @Nullable TabSelectionEditorNavigationProvider navigationProvider);

        /**
         * @return Whether the TabSelectionEditor is visible.
         */
        boolean isVisible();
    }

    /**
     * Provider of action for the navigation button in {@link TabSelectionEditorMediator}.
     */
    public static class TabSelectionEditorNavigationProvider {
        private final TabSelectionEditorCoordinator
                .TabSelectionEditorController mTabSelectionEditorController;
        private final Context mContext;

        public TabSelectionEditorNavigationProvider(Context context,
                TabSelectionEditorCoordinator
                        .TabSelectionEditorController tabSelectionEditorController) {
            mContext = context;
            mTabSelectionEditorController = tabSelectionEditorController;
        }

        /**
         * Defines what to do when the navigation button is clicked.
         */
        public void goBack() {
            TabUiMetricsHelper.recordSelectionEditorExitMetrics(
                    TabSelectionEditorExitMetricGroups.CLOSED_BY_USER, mContext);
            mTabSelectionEditorController.hide();
        }
    }

    private final Activity mActivity;
    private final ViewGroup mParentView;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabModelSelector mTabModelSelector;
    private final TabSelectionEditorLayout mTabSelectionEditorLayout;
    private final TabListCoordinator mTabListCoordinator;
    private final SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mTabSelectionEditorLayoutChangeProcessor;
    private final TabSelectionEditorMediator mTabSelectionEditorMediator;
    private final Callback<RecyclerViewPosition> mClientTabListRecyclerViewPositionSetter;
    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;

    public TabSelectionEditorCoordinator(Activity activity, ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            Callback<RecyclerViewPosition> clientTabListRecyclerViewPositionSetter,
            @TabListMode int mode, ViewGroup rootView, boolean displayGroups,
            SnackbarManager snackbarManager) {
        try (TraceEvent e = TraceEvent.scoped("TabSelectionEditorCoordinator.constructor")) {
            mActivity = activity;
            mParentView = parentView;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mTabModelSelector = tabModelSelector;
            mClientTabListRecyclerViewPositionSetter = clientTabListRecyclerViewPositionSetter;
            assert mode == TabListCoordinator.TabListMode.GRID
                    || mode == TabListCoordinator.TabListMode.LIST;

            mTabSelectionEditorLayout =
                    LayoutInflater.from(activity)
                            .inflate(R.layout.tab_selection_editor_layout, parentView, false)
                            .findViewById(R.id.selectable_list);

            ThumbnailProvider thumbnailProvider =
                    initThumbnailProvider(displayGroups, tabContentManager);
            PseudoTab.TitleProvider titleProvider = displayGroups ? this::getTitle : null;

            // TODO(ckitagawa): Lazily instantiate the TabSelectionEditorCoordinator. When doing so,
            // the Coordinator hosting the TabSelectionEditorCoordinator could share and reconfigure
            // its TabListCoordinator to work with the editor as an optimization.
            mTabListCoordinator =
                    new TabListCoordinator(mode, activity, mBrowserControlsStateProvider,
                            mTabModelSelector, thumbnailProvider, titleProvider, displayGroups,
                            null, null, TabProperties.UiType.SELECTABLE, this::getSelectionDelegate,
                            null, mTabSelectionEditorLayout, false, COMPONENT_NAME, rootView, null);

            // Note: The TabSelectionEditorCoordinator is always created after native is
            // initialized.
            assert LibraryLoader.getInstance().isInitialized();
            mTabListCoordinator.initWithNative(null);
            if (mMultiThumbnailCardProvider != null) {
                mMultiThumbnailCardProvider.initWithNative();
            }

            mTabListCoordinator.registerItemType(
                    TabProperties.UiType.DIVIDER,
                    new LayoutViewBuilder(R.layout.horizontal_divider),
                    (model, view, propertyKey) -> {});
            RecyclerView.LayoutManager layoutManager =
                    mTabListCoordinator.getContainerView().getLayoutManager();
            if (layoutManager instanceof GridLayoutManager) {
                ((GridLayoutManager) layoutManager)
                        .setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup() {
                            @Override
                            public int getSpanSize(int i) {
                                int itemType = mTabListCoordinator.getContainerView()
                                                       .getAdapter()
                                                       .getItemViewType(i);

                                if (itemType == TabProperties.UiType.DIVIDER) {
                                    return ((GridLayoutManager) layoutManager).getSpanCount();
                                }
                                return 1;
                            }
                        });
            }

            mTabSelectionEditorLayout.initialize(mParentView,
                    mTabListCoordinator.getContainerView(),
                    mTabListCoordinator.getContainerView().getAdapter(), mSelectionDelegate);
            mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

            mModel = new PropertyModel.Builder(TabSelectionEditorProperties.ALL_KEYS)
                             .with(IS_VISIBLE, false)
                             .build();

            mTabSelectionEditorLayoutChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mTabSelectionEditorLayout, TabSelectionEditorLayoutBinder::bind, false);

            ResetHandler resetHandler = new ResetHandler() {
                @Override
                public void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount,
                        @Nullable RecyclerViewPosition recyclerViewPosition, boolean quickMode) {
                    TabSelectionEditorCoordinator.this.resetWithListOfTabs(
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
                }
            };
            // TODO(crbug.com/1393679): Refactor SnackbarManager to support multiple overridden
            // parentViews in a stack to avoid contention and using new snackbar managers.
            mTabSelectionEditorMediator = new TabSelectionEditorMediator(mActivity,
                    mTabModelSelector, mTabListCoordinator, resetHandler, mModel,
                    mSelectionDelegate, mTabSelectionEditorLayout.getToolbar(), displayGroups,
                    snackbarManager, mTabSelectionEditorLayout);
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
     * @param tabs List of {@link Tab}s to reset.
     * @param preSelectedCount First {@code preSelectedCount} {@code tabs} are pre-selected.
     * @param quickMode whether to use quick mode.
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount, boolean quickMode) {
        mTabListCoordinator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), quickMode, /*mruMode=*/false);

        if (tabs != null && preSelectedCount > 0 && preSelectedCount < tabs.size()) {
            mTabListCoordinator.addSpecialListItem(preSelectedCount, TabProperties.UiType.DIVIDER,
                    new PropertyModel.Builder(CARD_TYPE).with(CARD_TYPE, OTHERS).build());
        }
    }

    private String getTitle(Context context, PseudoTab tab) {
        int numRelatedTabs = PseudoTab.getRelatedTabs(context, tab, mTabModelSelector).size();

        if (numRelatedTabs == 1) return tab.getTitle();

        return TabGroupTitleEditor.getDefaultTitle(context, numRelatedTabs);
    }

    private ThumbnailProvider initThumbnailProvider(
            boolean displayGroups, TabContentManager tabContentManager) {
        if (displayGroups) {
            mMultiThumbnailCardProvider = new MultiThumbnailCardProvider(
                    mActivity, mBrowserControlsStateProvider, tabContentManager, mTabModelSelector);
            return mMultiThumbnailCardProvider;
        }
        return (tabId, thumbnailSize, callback, forceUpdate, writeBack, isSelected) -> {
            tabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeBack);
        };
    }

    /**
     * @return {@link TabSelectionEditorController} that can control the TabSelectionEditor.
     */
    TabSelectionEditorController getController() {
        return mTabSelectionEditorMediator;
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.onDestroy();
        mTabSelectionEditorLayout.destroy();
        mTabSelectionEditorMediator.destroy();
        mTabSelectionEditorLayoutChangeProcessor.destroy();
        if (mMultiThumbnailCardProvider != null) {
            mMultiThumbnailCardProvider.destroy();
        }
    }

    /**
     * @return The {@link TabSelectionEditorLayout} for testing.
     */
    TabSelectionEditorLayout getTabSelectionEditorLayoutForTesting() {
        return mTabSelectionEditorLayout;
    }

    /**
     * @return The {@link TabListRecyclerView} for testing.
     */
    TabListRecyclerView getTabListRecyclerViewForTesting() {
        return mTabListCoordinator.getContainerView();
    }
}
