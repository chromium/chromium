// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.GridLayoutManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator to manage and display the Vertical Tab List. */
@NullMarked
public class VerticalTabListCoordinator {
    // TODO(crbug.com/515138646): Refactor this loose String component name to a unified @IntDef
    // enum system once TabListMediator's constructor and UMA logging are updated to support it.
    static final String COMPONENT_NAME = "VerticalTabs";
    static final int DEFAULT_GRID_SPAN_COUNT = 4;
    private final ViewGroup mContainerView;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private @Nullable TabListMediator mMediator;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable TabModelSelectorObserver mTabModelSelectorObserver;

    private class VerticalTabListClickHandler
            implements TabListMediator.GridCardOnClickListenerProvider {
        @Override
        public @Nullable TabActionListener onTabGroupClicked(Tab tab) {
            // TODO(crbug.com/509226293): expand/collapse
            return null;
        }

        @Override
        public @Nullable TabActionListener onTabGroupClicked(String syncId) {
            return null;
        }

        @Override
        public void onTabSelecting(int tabId, boolean fromActionButton) {
            TabModelSelector selector = mTabModelSelector;
            if (selector != null) {
                // TODO(crbug.com/509226293): Coordinate tab selection with smooth side panel
                // dismissal or collapse animations when running on narrow screens.
                TabModelUtils.selectTabById(selector, tabId, TabSelectionType.FROM_USER);
            }
        }
    }

    public VerticalTabListCoordinator(
            Activity activity, TabModelSelector tabModelSelector, Profile profile) {
        TabListModel modelList = new TabListModel();
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(modelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = modelList.get(position);
                        if (item.type == UiType.TAB && item.model.get(TabProperties.IS_PINNED)) {
                            return UiType.PINNED_TAB;
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
        // 2. Wire up footer container (R.id.vertical_tab_footer_container)
        // 3. Attach ItemTouchHelper for vertical row dragging & reordering.
        // 4. Register Right-click / Long-press Context Menu listener for tab interactions.
        // 5. Define a dedicated vertical_tab_group_header.xml layout and styling to handle
        // colorful background fills, expand/collapse chevron arrows, and group titles for
        // tab groups cleanly.

        mTabModelSelector = tabModelSelector;

        // TODO(crbug.com/509226293): Refactor the shared TabListMediator to dynamically manage
        // the display (inline vs. collapsed) and drag/drop reordering of tab group children.
        // Instead of using static boolean flags or string checks, we should decide whether to
        // act on a group or a single tab by checking the row's type (Group Header vs. Child Tab),
        // and ask the TabGroupModelFilter if a group is collapsed to decide whether to show
        // its child tabs inline.
        mMediator =
                new TabListMediator(
                        activity,
                        modelList,
                        TabListCoordinator.TabListMode.GRID,
                        /* modalDialogManager */ null,
                        tabModelSelector.getCurrentTabModelSupplier(),
                        /* thumbnailProvider */ null,
                        mTabListFaviconProvider,
                        /* actionOnRelatedTabs */ true,
                        /* selectionDelegateProvider */ null,
                        /* gridCardOnClickListenerProvider */ new VerticalTabListClickHandler(),
                        /* dialogHandler */ null,
                        /* priceWelcomeMessageControllerSupplier */ null,
                        COMPONENT_NAME,
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
                    public void onChange() {
                        TabModelSelector selector = mTabModelSelector;
                        if (selector != null && selector.isTabStateInitialized()) {
                            resetWithListOfTabs(selector.getCurrentModel());
                        }
                    }

                    @Override
                    public void onTabStateInitialized() {
                        TabModelSelector selector = mTabModelSelector;
                        if (selector != null) {
                            resetWithListOfTabs(selector.getCurrentModel());
                        }
                    }
                };
        tabModelSelector.addObserver(mTabModelSelectorObserver);

        if (tabModelSelector.isTabStateInitialized()) {
            resetWithListOfTabs(tabModelSelector.getCurrentModel());
        }
    }

    private void resetWithListOfTabs(@Nullable TabModel tabModel) {
        if (mMediator == null || tabModel == null) return;
        mMediator.resetWithListOfTabs(
                tabModel.getRepresentativeTabList(),
                /* tabGroupSyncIds */ null,
                /* quickMode */ false);
    }

    /** Returns the root ViewGroup container representing the Left Rail sidebar. */
    public View getView() {
        return mContainerView;
    }

    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        if (mTabModelSelector != null && mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelectorObserver = null;
        }
        mTabModelSelector = null;

        if (mTabListFaviconProvider != null) {
            mTabListFaviconProvider.destroy();
        }
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

    /** Returns the default grid column span count for the Left Rail. */
    private int getSpanCount() {
        // TODO(crbug.com/509226293): When the Left Rail becomes collapsible or resizable, the span
        // count must be calculated dynamically based on the measured width of the container.
        return DEFAULT_GRID_SPAN_COUNT;
    }
}
