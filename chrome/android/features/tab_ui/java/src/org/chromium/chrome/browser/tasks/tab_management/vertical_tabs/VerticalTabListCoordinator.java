// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabComponentId;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListConfigDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.function.Supplier;

/** Coordinator to manage and display the Vertical Tab List. */
@NullMarked
public class VerticalTabListCoordinator {
    static final int DEFAULT_GRID_SPAN_COUNT = 4;
    private final ViewGroup mContainerView;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModelList;
    private final TabListMediator mMediator;
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;

    private class VerticalTabListClickHandler implements TabListItemOnClickListenerProvider {
        private final TabActionListener mTabGroupClickedListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        toggleTabGroupExpansion(tabId);
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        // Intentional no-op.
                    }
                };

        @Override
        public @Nullable TabActionListener onTabGroupClicked(Tab tab) {
            return mTabGroupClickedListener;
        }

        @Override
        public @Nullable TabActionListener onTabGroupClicked(String syncId) {
            return null;
        }

        @Override
        public void onTabSelecting(int tabId, boolean fromActionButton) {
            // TODO(crbug.com/509226293): Coordinate tab selection with smooth side panel
            // dismissal or collapse animations when running on narrow screens.
            TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER);
        }

        @Override
        public @Nullable Boolean isTabGroupSelected(Tab tab, PropertyModel model) {
            // In Vertical Tabs, the Group Header card acts strictly as an expandable accordion
            // header, and is never selectable (individual child webpage rows show active
            // highlights).
            return false;
        }

        @Override
        public @Nullable TabActionButtonData getTabGroupActionButtonData(
                Tab tab,
                PropertyModel model,
                Supplier<TabActionListener> defaultOverflowListenerSupplier) {
            // Vertical Tabs group header cards act strictly as accordion expansion toggles
            // and do not display any action button (neither close nor overflow menu).
            return null;
        }
    }

    public VerticalTabListCoordinator(
            Activity activity, TabModelSelector tabModelSelector, Profile profile) {
        mModelList = new TabListModel();
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(mModelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = mModelList.get(position);
                        if (item.type == UiType.TAB) {
                            if (item.model.get(TabProperties.IS_PINNED)) {
                                return UiType.PINNED_TAB;
                            } else if (item.model.get(TabProperties.TAB_GROUP_HEADER_ID) != null) {
                                return UiType.TAB_GROUP;
                            }
                        }
                        return super.getItemViewType(position);
                    }
                };

        adapter.registerType(
                UiType.TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(R.layout.vertical_tab_item, parent, false),
                TabVerticalViewBinder::bindTab);

        adapter.registerType(
                UiType.PINNED_TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(R.layout.vertical_tab_pinned_item, parent, false),
                TabVerticalViewBinder::bindPinnedTab);

        adapter.registerType(
                UiType.TAB_GROUP,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(R.layout.vertical_tab_group_header, parent, false),
                TabVerticalViewBinder::bindTabGroupHeader);

        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mContainerView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        TabListRecyclerView recyclerView = mContainerView.findViewById(R.id.tab_list_recycler_view);

        GridLayoutManager layoutManager = createGridLayoutManager(activity, adapter);

        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);
        recyclerView.setVisibility(View.VISIBLE);

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        activity,
                        /* isTabStrip */ false,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);

        // TODO(crbug.com/509226293):
        // 1. Wire up header container (R.id.vertical_tab_header_container) for search & grid
        // buttons.
        // 3. Attach ItemTouchHelper for vertical row dragging & reordering.
        // 4. Register Right-click / Long-press Context Menu listener for tab interactions.

        mTabModelSelector = tabModelSelector;

        TabListConfigDelegate tabListConfigDelegate =
                new TabListConfigDelegate() {
                    @Override
                    public boolean supportsNestedTabGroups() {
                        return true;
                    }

                    @Override
                    public boolean shouldActOnRelatedTabs() {
                        return true;
                    }

                    @Override
                    public boolean supportsMessageCards() {
                        return false;
                    }
                };

        ImageButton newTabButton = mContainerView.findViewById(R.id.new_tab_button);
        newTabButton.setOnClickListener(v -> handleNewTabButtonClick());

        mMediator =
                new TabListMediator(
                        activity,
                        mModelList,
                        TabListCoordinator.TabListMode.GRID,
                        /* modalDialogManager */ null,
                        tabModelSelector.getCurrentTabModelSupplier(),
                        /* thumbnailProvider */ null,
                        mTabListFaviconProvider,
                        /* selectionDelegateProvider */ null,
                        new VerticalTabListClickHandler(),
                        tabListConfigDelegate,
                        /* dialogHandler */ null,
                        /* priceWelcomeMessageControllerSupplier */ null,
                        TabComponentId.VERTICAL_TABS,
                        TabProperties.TabActionState.CLOSABLE,
                        /* dataSharingTabManager */ null,
                        /* onTabGroupCreation */ null,
                        /* undoBarExplicitTrigger */ null,
                        /* snackbarManager */ null,
                        TabListEditorCoordinator.UNLIMITED_SELECTION,
                        /* isSingleContextMode */ false,
                        /* onDragStateChangedListener */ () -> {});

        mMediator.initWithNative(profile.getOriginalProfile());

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabStateInitialized() {
                        resetWithListOfTabs(mTabModelSelector.getCurrentModel());
                    }
                };
        tabModelSelector.addObserver(mTabModelSelectorObserver);

        mCurrentTabModelObserver = this::onCurrentTabModelChanged;
        tabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndCallIfNonNull(mCurrentTabModelObserver);
    }

    /** Returns the root ViewGroup container representing the Left Rail sidebar. */
    public View getView() {
        return mContainerView;
    }

    public void destroy() {
        mMediator.destroy();
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);

        if (mTabListFaviconProvider != null) {
            mTabListFaviconProvider.destroy();
        }
    }

    /**
     * Toggles the expanded/collapsed visual and layout state of a tab group.
     *
     * @param tabId the ID of the representative tab representing the tab group.
     */
    @VisibleForTesting
    void toggleTabGroupExpansion(int tabId) {
        mMediator.toggleTabGroupExpansion(tabId);
    }

    private void onCurrentTabModelChanged(TabModel tabModel) {
        if (mTabModelSelector.isTabStateInitialized()) {
            resetWithListOfTabs(tabModel);
        }
    }

    private void resetWithListOfTabs(@Nullable TabModel tabModel) {
        if (tabModel == null) return;

        mMediator.resetWithListOfTabs(
                tabModel.getRepresentativeTabList(),
                /* tabGroupSyncIds */ null,
                /* quickMode */ false);
    }

    private GridLayoutManager createGridLayoutManager(
            Activity activity, SimpleRecyclerViewAdapter adapter) {
        GridLayoutManager layoutManager = new GridLayoutManager(activity, getSpanCount());
        // Custom SpanSizeLookup: Pinned tabs take 1 column, regular tabs span the full grid width
        layoutManager.setSpanSizeLookup(
                new GridLayoutManager.SpanSizeLookup() {
                    @Override
                    public int getSpanSize(int position) {
                        int type = adapter.getItemViewType(position);
                        if (type == UiType.PINNED_TAB) {
                            return 1;
                        }
                        return layoutManager.getSpanCount();
                    }
                });
        return layoutManager;
    }

    private void handleNewTabButtonClick() {
        TabModel model = mTabModelSelector.getCurrentModel();

        if (!model.isIncognitoBranded()) model.commitAllTabClosures();
        TabCreatorUtil.launchNtp(model.getTabCreator());
    }

    /** Returns the default grid column span count for the Left Rail. */
    private int getSpanCount() {
        // TODO(crbug.com/509226293): When the Left Rail becomes collapsible or resizable, the span
        // count must be calculated dynamically based on the measured width of the container.
        return DEFAULT_GRID_SPAN_COUNT;
    }
}
