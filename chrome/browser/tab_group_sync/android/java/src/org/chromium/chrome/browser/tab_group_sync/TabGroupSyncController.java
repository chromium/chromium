// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Central class responsible for making things happen. i.e. apply remote changes to local and local
 * changes to remote.
 */
public final class TabGroupSyncController {
    /**
     * A delegate in helping out with creating and navigating tabs in response to remote updates
     * from sync. The tab will be created in a background state and will not be navigated
     * immediately. The navigation will happen only when the tab becomes active such as user
     * switches to the tab.
     */
    public interface TabCreationDelegate {

        /**
         * Creates a tab in background in the local tab model. The tab will be created at the given
         * position and will be loaded with the given URL. The URL will not be loaded right away.
         *
         * @param url The URL to load.
         * @param parent The parent of the tab.
         * @param position The position of the tab in the tab model.
         * @return The tab created.
         */
        Tab createBackgroundTab(GURL url, Tab parent, int position);
    }
}
