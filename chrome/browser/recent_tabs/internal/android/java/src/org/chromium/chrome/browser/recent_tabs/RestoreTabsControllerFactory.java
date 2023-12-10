// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** A factory interface for building a RestoreTabsController instance. */
public class RestoreTabsControllerFactory {
    /**
     * @return An instance of RestoreTabsController.
     */
    public static RestoreTabsController createInstance(
            Context context,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        return new RestoreTabsControllerImpl(
                context, profile, tabCreatorManager, bottomSheetController);
    }
}
