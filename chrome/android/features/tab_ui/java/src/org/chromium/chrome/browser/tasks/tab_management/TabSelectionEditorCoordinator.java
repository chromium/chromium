// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;
import static org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorProperties.IS_VISIBLE;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.tab_ui.R;
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

    /**
     * An interface to control the TabSelectionEditor.
     */
    interface TabSelectionEditorController {
        /**
         * Shows the TabSelectionEditor with the given {@link Tab}s.
         * @param tabs List of {@link Tab}s to show.
         */
        void show(List<Tab> tabs);

        /**
         * Shows the TabSelectionEditor with the given {@Link Tab}s, and the first
         * {@code preSelectedTabCount} tabs being selected.
         * @param tabs List of {@link Tab}s to show.
         * @param preSelectedTabCount Number of selected {@link Tab}s.
         */
        void show(List<Tab> tabs, int preSelectedTabCount);

        /**
         * Hides the TabSelectionEditor.
         */
        void hide();

        /**
         * @return Whether or not the TabSelectionEditor consumed the event.
         */
        boolean handleBackPressed();

        /**
         * Configure the Toolbar for TabSelectionEditor. The default button text is "Group".
         * @param actionButtonText Button text for the action button.
         * @param actionButtonDescriptionResourceId Content description template resource Id for the
         *         action button. This should be in a plurals form.
         * @param actionProvider The {@link TabSelectionEditorActionProvider} that specifies the
         *         action when action button gets clicked.
         * @param actionButtonEnablingThreshold The minimum threshold to enable the action button.
         *         If it's -1 use the default value.
         * @param navigationProvider The {@link TabSelectionEditorNavigationProvider} that specifies
         */
        void configureToolbar(@Nullable String actionButtonText,
                @Nullable Integer actionButtonDescriptionResourceId,
                @Nullable TabSelectionEditorActionProvider actionProvider,
                int actionButtonEnablingThreshold,
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

        public TabSelectionEditorNavigationProvider(
                TabSelectionEditorCoordinator
                        .TabSelectionEditorController tabSelectionEditorController) {
            mTabSelectionEditorController = tabSelectionEditorController;
        }

        /**
         * Defines what to do when the navigation button is clicked.
         */
        public void goBack() {
            RecordUserAction.record("TabMultiSelect.Cancelled");
            mTabSelectionEditorController.hide();
        }
    }

    private final Context mContext;
    private final ViewGroup mParentView;
    private final TabModelSelector mTabModelSelector;
    private final TabSelectionEditorLayout mTabSelectionEditorLayout;
    private final TabListCoordinator mTabListCoordinator;
    private final SelectionDelegate<Integer> mSelectionDelegate = new SelectionDelegate<>();
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mTabSelectionEditorLayoutChangeProcessor;
    private final TabSelectionEditorMediator mTabSelectionEditorMediator;

    public TabSelectionEditorCoordinator(Context context, ViewGroup parentView,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            @TabListMode int mode) {
        mContext = context;
        mParentView = parentView;
        mTabModelSelector = tabModelSelector;
        assert mode == TabListCoordinator.TabListMode.GRID
                || mode == TabListCoordinator.TabListMode.LIST;

        mTabSelectionEditorLayout =
                LayoutInflater.from(context)
                        .inflate(R.layout.tab_selection_editor_layout, parentView, false)
                        .findViewById(R.id.selectable_list);

        mTabListCoordinator = new TabListCoordinator(mode, context, mTabModelSelector,
                tabContentManager::getTabThumbnailWithCallback, null, false, null, null,
                TabProperties.UiType.SELECTABLE, this::getSelectionDelegate, null,
                mTabSelectionEditorLayout, false, COMPONENT_NAME);

        // Note: The TabSelectionEditorCoordinator is always created after native is initialized.
        assert LibraryLoader.getInstance().isInitialized();
        mTabListCoordinator.initWithNative(null);

        mTabListCoordinator.registerItemType(TabProperties.UiType.DIVIDER,
                new LayoutViewBuilder(R.layout.divider_preference),
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

        mTabSelectionEditorLayout.initialize(mParentView, mTabListCoordinator.getContainerView(),
                mTabListCoordinator.getContainerView().getAdapter(), mSelectionDelegate);
        mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);

        mModel = new PropertyModel.Builder(TabSelectionEditorProperties.ALL_KEYS)
                         .with(IS_VISIBLE, false)
                         .build();

        mTabSelectionEditorLayoutChangeProcessor = PropertyModelChangeProcessor.create(
                mModel, mTabSelectionEditorLayout, TabSelectionEditorLayoutBinder::bind, false);

        mTabSelectionEditorMediator = new TabSelectionEditorMediator(
                mContext, mTabModelSelector, this::resetWithListOfTabs, mModel, mSelectionDelegate);
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
     */
    void resetWithListOfTabs(@Nullable List<Tab> tabs, int preSelectedCount) {
        mTabListCoordinator.resetWithListOfTabs(tabs);

        if (tabs != null && preSelectedCount > 0 && preSelectedCount < tabs.size()) {
            mTabListCoordinator.addSpecialListItem(preSelectedCount, TabProperties.UiType.DIVIDER,
                    new PropertyModel.Builder(CARD_TYPE).with(CARD_TYPE, OTHERS).build());
        }
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
        mTabListCoordinator.destroy();
        mTabSelectionEditorLayout.destroy();
        mTabSelectionEditorMediator.destroy();
        mTabSelectionEditorLayoutChangeProcessor.destroy();
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
