// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Resources;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

import java.util.List;

/**
 * A coordinator for the context menu on the tab strip by long-pressing on a multi selected tab. It
 * is responsible for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class MultiSelectedTabsContextMenuCoordinator
        extends TabOverflowMenuCoordinator<List<Integer>> {
    private final TabModel mTabModel;
    private final WindowAndroid mWindowAndroid;

    private MultiSelectedTabsContextMenuCoordinator(
            TabModel tabModel,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(
                        () -> tabModel,
                        tabGroupModelFilter,
                        tabGroupListBottomSheetCoordinator,
                        multiInstanceManager),
                () -> tabModel,
                tabGroupSyncService,
                collaborationService,
                windowAndroid.getActivity().get());
        mTabModel = tabModel;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Creates MultiSelectedTabsContextMenuCoordinator object.
     *
     * @param tabModel {@link TabModel}.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to act on.
     * @param tabGroupListBottomSheetCoordinator The {@link TabGroupListBottomSheetCoordinator} that
     *     will be used to show a bottom sheet when the user selects the "Add to group" option.
     * @param multiInstanceManager The {@link MultiInstanceManager} that will be used to move tabs
     *     from one window to another.
     * @param windowAndroid The {@link WindowAndroid} where this context menu will be shown.
     */
    public static MultiSelectedTabsContextMenuCoordinator createContextMenuCoordinator(
            TabModel tabModel,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager,
            WindowAndroid windowAndroid) {
        Profile profile = assumeNonNull(tabModel.getProfile());

        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);

        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new MultiSelectedTabsContextMenuCoordinator(
                tabModel,
                tabGroupModelFilter,
                tabGroupListBottomSheetCoordinator,
                multiInstanceManager,
                windowAndroid,
                tabGroupSyncService,
                collaborationService);
    }

    @VisibleForTesting
    static OnItemClickedCallback<List<Integer>> getMenuItemClickedCallback(
            Supplier<TabModel> tabModelSupplier,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator,
            MultiInstanceManager multiInstanceManager) {
        return (menuId, tabIds, collaborationId, listViewTouchTracker) -> {
            assert !tabIds.isEmpty() : "Empty tab id list provided";
        };
    }

    /**
     * Show the context menu for multi selected tabs.
     *
     * @param anchorViewRectProvider The context menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabIds The tab ids of the interacting multi selected tabs.
     */
    protected void showMenu(RectProvider anchorViewRectProvider, List<Integer> tabIds) {
        createAndShowMenu(
                anchorViewRectProvider,
                tabIds,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                /* animStyle= */ Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                assumeNonNull(mWindowAndroid.getActivity().get()));
        RecordUserAction.record("MobileToolbarTabMenu.Shown");
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, List<Integer> ids) {
        assert !ids.isEmpty() : "Empty ids list provided";
        assert mTabModel != null : "TabModel cannot be null";
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return MathUtils.clamp(
                anchorViewWidthPx,
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(List<Integer> ids) {
        // Multi-select does not support collaboration yet.
        return null;
    }
}
