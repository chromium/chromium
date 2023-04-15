// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;

/**
 * A factory interface for building a RestoreTabsController instance.
 */
public class RestoreTabsControllerFactory {
    /**
     * A listener to indicate the lifecycle status of the RestoreTabs feature.
     */
    public interface ControllerListener {
        /**
         * Action to perform when the restore tabs promo is done showing.
         */
        public void onDismissed();
    }

    /**
     * @return An instance of RestoreTabsControllerImpl.
     */
    public static RestoreTabsControllerImpl createInstance(Profile profile,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager) {
        return new RestoreTabsControllerImpl(profile, listener, tabCreatorManager);
    }
}
