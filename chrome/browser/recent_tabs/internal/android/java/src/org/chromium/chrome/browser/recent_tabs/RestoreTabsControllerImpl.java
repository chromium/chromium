// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/** Controller for accessing helper functions for the singleton factory instance. */
public class RestoreTabsControllerImpl implements RestoreTabsController {
    private RestoreTabsCoordinator mRestoreTabsCoordinator;

    public RestoreTabsControllerImpl(
            Context context,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        mRestoreTabsCoordinator =
                new RestoreTabsCoordinator(
                        context, profile, tabCreatorManager, bottomSheetController);
    }

    @Override
    public void destroy() {
        mRestoreTabsCoordinator.destroy();
        mRestoreTabsCoordinator = null;
    }

    @Override
    public void showHomeScreen(
            ForeignSessionHelper foreignSessionHelper,
            List<ForeignSession> sessions,
            RestoreTabsControllerDelegate delegate) {
        mRestoreTabsCoordinator.showHomeScreen(foreignSessionHelper, sessions, delegate);
    }
}
