// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
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
     * @param callback Used to follow up on tab group creation.
     * @param filter Used to read current tab groups.
     * @param controller Used to interact with the bottom sheet.
     * @param showNewGroupRow Whether the 'New Tab Group' row should be displayed.
     */
    TabGroupListBottomSheetCoordinator create(
            Context context,
            Profile profile,
            TabGroupCreationCallback callback,
            TabGroupModelFilter filter,
            BottomSheetController controller,
            boolean showNewGroupRow);
}
