// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/**
 * A coordinator for the overflow menu in tab groups. This applies to both the TabGridDialog toolbar
 * and tab group cards on GTS. It is responsible for creating a list of menu items, setting up the
 * menu and displaying the menu.
 */
@NullMarked
public abstract class TabGroupOverflowMenuCoordinator extends TabOverflowMenuCoordinator<Token> {

    /**
     * @param menuLayout The menu layout to use.
     * @param onItemClickedCallback A callback for listening to clicks.
     * @param tabModelSupplier The supplier of the tab model.
     * @param tabGroupSyncService Used to checking if a group is shared or synced.
     * @param collaborationService Used for checking the user is the owner of a group.
     * @param context The {@link Context} that the coordinator resides in.
     */
    protected TabGroupOverflowMenuCoordinator(
            int menuLayout,
            OnItemClickedCallback<Token> onItemClickedCallback,
            Supplier<TabModel> tabModelSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            CollaborationService collaborationService,
            Context context) {
        super(
                menuLayout,
                onItemClickedCallback,
                tabModelSupplier,
                tabGroupSyncService,
                collaborationService,
                context);
    }

    @Override
    protected @Nullable String getCollaborationIdOrNull(Token tabGroupId) {
        return TabShareUtils.getCollaborationIdOrNull(tabGroupId, mTabGroupSyncService);
    }

    public void setTabGroupSyncServiceForTesting(TabGroupSyncService tabGroupSyncService) {
        mTabGroupSyncService = tabGroupSyncService;
    }
}
