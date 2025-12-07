// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.ANIMATION_MANAGER;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripProperties.SCROLL_TO_POSITION;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;

/**
 * The coordinator for the pinned tabs strip, which is a RecyclerView that shows a list of pinned
 * tabs.
 */
@NullMarked
public class PinnedTabStripCoordinator {
    private final PinnedTabStripMediator mMediator;
    private final TabListRecyclerView mPinnedTabsRecyclerView;
    private final ObservableSupplierImpl<Boolean> mIsVisibilityAnimationRunningSupplier;
    private final PinnedTabStripAnimationManager mAnimationManager;

    /**
     * Constructor for PinnedTabStripCoordinator.
     *
     * @param activity The current activity.
     * @param parentView The parent view to attach the pinned tabs strip to.
     * @param tabListCoordinator The coordinator for the main tab grid.
     * @param tabGroupModelFilterSupplier The supplier of the current {@link TabGroupModelFilter}.
     */
    public PinnedTabStripCoordinator(
            Activity activity,
            ViewGroup parentView,
            TabListCoordinator tabListCoordinator,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable onTabGroupCreation) {
        mPinnedTabsRecyclerView =
                (TabListRecyclerView)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.pinned_tab_strip_recycler_view_layout,
                                        parentView,
                                        /* attachToParent= */ false);
        TabListModel pinnedTabsModelList = new TabListModel();
        mIsVisibilityAnimationRunningSupplier = new ObservableSupplierImpl<>(false);
        mAnimationManager = new PinnedTabStripAnimationManager(mPinnedTabsRecyclerView);
        PropertyModel pinnedTabStripPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(IS_VISIBLE, false)
                        .with(SCROLL_TO_POSITION, -1)
                        .with(ANIMATION_MANAGER, mAnimationManager)
                        .with(
                                IS_VISIBILITY_ANIMATION_RUNNING_SUPPLIER,
                                mIsVisibilityAnimationRunningSupplier)
                        .build();

        // Setup the adapter.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(pinnedTabsModelList);
        adapter.registerType(
                UiType.TAB,
                parent -> createPinnedTabStripItemView(activity, parent),
                PinnedTabStripItemViewBinder::bind);

        LinearLayoutManager layoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        mPinnedTabsRecyclerView.setLayoutManager(layoutManager);
        mPinnedTabsRecyclerView.setAdapter(adapter);

        PropertyModelChangeProcessor.create(
                pinnedTabStripPropertyModel,
                mPinnedTabsRecyclerView,
                PinnedTabStripViewBinder::bind);

        RecyclerView tabGridListRecyclerView = tabListCoordinator.getContainerView();
        TabListModel tabListModel = tabListCoordinator.getTabListModel();
        mMediator =
                createMediator(
                        activity,
                        tabGridListRecyclerView,
                        tabListCoordinator,
                        tabListModel,
                        pinnedTabsModelList,
                        pinnedTabStripPropertyModel,
                        tabGroupModelFilterSupplier,
                        tabBookmarkerSupplier,
                        bottomSheetController,
                        modalDialogManager,
                        onTabGroupCreation);

        PinnedTabStripItemTouchHelperCallback callback =
                new PinnedTabStripItemTouchHelperCallback(
                        activity,
                        tabGroupModelFilterSupplier,
                        pinnedTabsModelList,
                        () -> mPinnedTabsRecyclerView,
                        mMediator::onLongPress);

        ItemTouchHelper2 itemTouchHelper = createItemTouchHelper(callback);
        itemTouchHelper.attachToRecyclerView(mPinnedTabsRecyclerView);
    }

    @VisibleForTesting
    ItemTouchHelper2 createItemTouchHelper(PinnedTabStripItemTouchHelperCallback callback) {
        return new ItemTouchHelper2(callback);
    }

    /** Returns the {@link TabListRecyclerView} for the pinned tabs strip. */
    public TabListRecyclerView getPinnedTabsRecyclerView() {
        return mPinnedTabsRecyclerView;
    }

    /** Returns a supplier that indicates whether the pinned tab strip is animating. */
    public ObservableSupplier<Boolean> getIsVisibilityAnimationRunningSupplier() {
        return mIsVisibilityAnimationRunningSupplier;
    }

    /** Called when the pinned tabs strip is scrolled. */
    public void onScrolled() {
        mMediator.onScrolled();
    }

    /** Returns whether the pinned tabs strip is currently visible. */
    public boolean isPinnedTabsBarVisible() {
        return mMediator.isPinnedTabsBarVisible();
    }

    PinnedTabStripMediator createMediator(
            Activity activity,
            RecyclerView tabGridListRecyclerView,
            TabListCoordinator tabListCoordinator,
            TabListModel tabListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable onTabGroupCreation) {
        GridLayoutManager tabGridListLayoutManager =
                (GridLayoutManager) tabGridListRecyclerView.getLayoutManager();
        assumeNonNull(tabGridListLayoutManager);
        return new PinnedTabStripMediator(
                activity,
                tabGridListLayoutManager,
                tabListCoordinator,
                tabListModel,
                pinnedTabsModelList,
                stripPropertyModel,
                tabGroupModelFilterSupplier,
                tabBookmarkerSupplier,
                bottomSheetController,
                modalDialogManager,
                onTabGroupCreation);
    }

    private static PinnedTabStripItemView createPinnedTabStripItemView(
            Activity activity, ViewGroup parent) {
        return (PinnedTabStripItemView)
                LayoutInflater.from(activity)
                        .inflate(
                                R.layout.pinned_tab_strip_item,
                                parent,
                                /* attachToParent= */ false);
    }

    public void destroy() {
        mMediator.destroy();
    }
}
