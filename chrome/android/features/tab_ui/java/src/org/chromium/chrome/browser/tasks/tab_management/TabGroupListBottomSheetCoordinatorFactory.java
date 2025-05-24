// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabMovedCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Factory class for creating {@link TabGroupListBottomSheetCoordinator} instances. */
@NullMarked
@FunctionalInterface
public interface TabGroupListBottomSheetCoordinatorFactory {
    /**
     * Creates a {@link TabGroupListBottomSheetCoordinator}.
     *
     * @param context The {@link Context} to attach the bottom sheet to.
     * @param profile The current user profile.
     * @param tabGroupCreationCallback Used to follow up on tab group creation.
     * @param tabMovedCallback Used to follow up on a tab being moved groups or ungrouped.
     * @param filter Used to read current tab groups.
     * @param controller Used to interact with the bottom sheet.
     * @param supportsShowNewGroup Whether the 'New Tab Group' row is supported.
     * @param destroyOnHide Whether the coordinator should be destroyed on hide.
     */
    TabGroupListBottomSheetCoordinator create(
            Context context,
            Profile profile,
            TabGroupCreationCallback tabGroupCreationCallback,
            @Nullable TabMovedCallback tabMovedCallback,
            TabGroupModelFilter filter,
            BottomSheetController controller,
            boolean supportsShowNewGroup,
            boolean destroyOnHide);
}
