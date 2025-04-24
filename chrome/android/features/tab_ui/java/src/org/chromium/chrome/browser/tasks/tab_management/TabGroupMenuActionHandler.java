// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** Handles the actions for the tab group related menu items in the app menu. */
@NullMarked
public class TabGroupMenuActionHandler {
    private final Context mContext;
    private final TabGroupModelFilter mFilter;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final Profile mProfile;
    private final TabGroupListBottomSheetCoordinatorFactory mFactory;

    private final ObservableSupplierImpl<TabGroupListBottomSheetCoordinator>
            mTabGroupListBottomSheetCoordinatorSupplier = new ObservableSupplierImpl<>();

    /**
     * @param context The context for the app menu.
     * @param filter Used to interact with tab groups.
     * @param bottomSheetController For interacting with the bottom sheet.
     * @param modalDialogManager For showing the tab group creation dialog.
     * @param profile The current profile.
     */
    public TabGroupMenuActionHandler(
            Context context,
            TabGroupModelFilter filter,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            Profile profile) {
        this(
                context,
                filter,
                bottomSheetController,
                modalDialogManager,
                profile,
                TabGroupListBottomSheetCoordinator::new);
    }

    @VisibleForTesting
    TabGroupMenuActionHandler(
            Context context,
            TabGroupModelFilter filter,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            Profile profile,
            TabGroupListBottomSheetCoordinatorFactory factory) {
        mContext = context;
        mFilter = filter;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mProfile = profile;
        mFactory = factory;
    }

    /**
     * Handles the "Add to group" action for the given tab.
     *
     * <p>If no tab groups exist, it creates a new group with the given tab.
     *
     * <p>If tab groups exist, it shows a bottom sheet allowing the user to select an existing group
     * or create a new one.
     *
     * @param tab The tab to be added to a group.
     */
    public void handleAddToGroupAction(Tab tab) {
        if (mFilter.getTabGroupCount() == 0) {
            mFilter.createSingleTabGroup(tab);
            @Nullable Token groupId = tab.getTabGroupId();
            if (groupId != null) {
                onTabGroupCreation(groupId);
            }
        } else {
            TabGroupListBottomSheetCoordinator tabGroupListBottomSheetCoordinator =
                    mFactory.create(
                            mContext,
                            mProfile,
                            this::onTabGroupCreation,
                            /* tabMovedCallback= */ null,
                            mFilter,
                            mBottomSheetController,
                            true,
                            true);
            mTabGroupListBottomSheetCoordinatorSupplier.set(tabGroupListBottomSheetCoordinator);
            tabGroupListBottomSheetCoordinator.showBottomSheet(List.of(tab));
        }
    }

    /**
     * Called after a tab group creation event. Shows the tab group creation dialog and cleans
     * unused classes if required.
     */
    private void onTabGroupCreation(Token tabGroupId) {
        TabGroupCreationDialogManager manager =
                new TabGroupCreationDialogManager(mContext, mModalDialogManager, null);
        manager.showDialog(tabGroupId, mFilter);
    }
}
