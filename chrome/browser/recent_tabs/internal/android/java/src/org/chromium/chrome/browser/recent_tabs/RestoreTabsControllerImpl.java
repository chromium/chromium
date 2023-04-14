// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;

/**
 * Controller for accessing helper functions for the singleton factory instance.
 */
public class RestoreTabsControllerImpl {
    private RestoreTabsFeatureHelper mHelper;
    private RestoreTabsCoordinator mRestoreTabsCoordinator;

    public RestoreTabsControllerImpl(Profile profile,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager) {
        mHelper = new RestoreTabsFeatureHelperImpl();
        mRestoreTabsCoordinator = new RestoreTabsCoordinator(profile, listener, tabCreatorManager);
    }

    public void destroy() {
        mRestoreTabsCoordinator.destroy();
        mRestoreTabsCoordinator = null;
        mHelper = null;
    }

    public RestoreTabsFeatureHelper getFeatureHelper() {
        return mHelper;
    }

    public void showBottomSheet() {
        mRestoreTabsCoordinator.showOptions();
    }
}