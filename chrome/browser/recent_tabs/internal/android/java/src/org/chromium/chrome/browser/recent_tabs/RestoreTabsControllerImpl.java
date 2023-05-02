// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Controller for accessing helper functions for the singleton factory instance.
 */
public class RestoreTabsControllerImpl {
    private RestoreTabsFeatureHelper mHelper;
    private RestoreTabsCoordinator mRestoreTabsCoordinator;

    public RestoreTabsControllerImpl(Context context, Profile profile,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager, BottomSheetController bottomSheetController) {
        mHelper = new RestoreTabsFeatureHelperImpl();
        mRestoreTabsCoordinator = new RestoreTabsCoordinator(
                context, profile, listener, tabCreatorManager, bottomSheetController);
    }

    public void destroy() {
        mRestoreTabsCoordinator.destroy();
        mRestoreTabsCoordinator = null;
        mHelper = null;
    }

    public RestoreTabsFeatureHelper getFeatureHelper() {
        return mHelper;
    }

    public void showHomeScreen() {
        mRestoreTabsCoordinator.showHomeScreen();
    }
}