// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Resources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

/**
 * A coordinator for the overflow menu accessed by long-pressing on a pinned tab. It is responsible
 * for creating a list of menu items, setting up the menu, and displaying the menu.
 */
@NullMarked
public class PinnedTabStripItemContextMenuCoordinator
        extends TabOverflowMenuCoordinator<@TabId Integer> {
    private final Activity mActivity;

    /**
     * Constructs a new {@link PinnedTabStripItemContextMenuCoordinator}.
     *
     * @param activity The {@link Activity} context.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to get the tab model.
     * @param tabGroupSyncService The {@link TabGroupSyncService} to handle tab group sync.
     * @param collaborationService The {@link CollaborationService} for handling collaboration.
     */
    PinnedTabStripItemContextMenuCoordinator(
            Activity activity,
            TabGroupModelFilter tabGroupModelFilter,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService) {
        super(
                R.layout.tab_switcher_action_menu_layout,
                getMenuItemClickedCallback(),
                tabGroupModelFilter::getTabModel,
                /* multiInstanceManager= */ null,
                tabGroupSyncService,
                collaborationService,
                activity);
        mActivity = activity;
    }

    /**
     * Show the overflow menu of the tab.
     *
     * @param anchorViewRectProvider The menu's anchor view rect provider. These are screen
     *     coordinates.
     * @param tabId The tab id of the interacting tab.
     */
    public void showMenu(RectProvider anchorViewRectProvider, int tabId) {
        boolean isIncognito = mTabModelSupplier.get().isIncognitoBranded();
        createAndShowMenu(
                anchorViewRectProvider,
                tabId,
                /* horizontalOverlapAnchor= */ true,
                /* verticalOverlapAnchor= */ false,
                Resources.ID_NULL,
                HorizontalOrientation.LAYOUT_DIRECTION,
                mActivity,
                isIncognito);
    }

    @Override
    protected void buildMenuActionItems(ModelList itemList, Integer tabId) {
        // TODO(crbug.com/445195867): Build menu items.
    }

    @Nullable
    @Override
    protected String getCollaborationIdOrNull(Integer id) {
        return null;
    }

    @Override
    protected int getMenuWidth(int anchorViewWidthPx) {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.tab_grid_context_menu_max_width);
    }

    static OnItemClickedCallback<Integer> getMenuItemClickedCallback() {
        return (menuId, tabId, collaborationId, listViewTouchTracker) -> {
            // TODO(crbug.com/445195867): Implement menu item clicks.
        };
    }

    public static PinnedTabStripItemContextMenuCoordinator createContextMenuCoordinator(
            Activity activity, TabGroupModelFilter tabGroupModelFilter) {
        Profile profile = assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
        @Nullable TabGroupSyncService tabGroupSyncService =
                profile.isOffTheRecord() ? null : TabGroupSyncServiceFactory.getForProfile(profile);
        CollaborationService collaborationService =
                CollaborationServiceFactory.getForProfile(profile);

        return new PinnedTabStripItemContextMenuCoordinator(
                activity, tabGroupModelFilter, tabGroupSyncService, collaborationService);
    }
}
