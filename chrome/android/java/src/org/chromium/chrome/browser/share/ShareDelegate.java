// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareParams;

/**
 * Interface to expose sharing to external classes.
 */
public interface ShareDelegate {
    /**
     * Initiate a share based on the provided ShareParams.
     *
     * @param params The share parameters.
     * @param chromeShareExtras The extras not contained in {@code params}.
     * @param shareOrigin Where the share originated.
     */
    void share(
            ShareParams params, ChromeShareExtras chromeShareExtras, @ShareOrigin int shareOrigin);

    /**
     * Initiate a share for the provided Tab.
     *
     * @param currentTab The Tab to be shared.
     * @param shareDirectly If this share should be sent directly to the last used share target.
     * @param shareOrigin Where the share originated.
     */
    void share(Tab currentTab, boolean shareDirectly, @ShareOrigin int shareOrigin);

    /**
     * Check if the custom share sheet is enabled.
     */
    boolean isSharingHubV1Enabled();

    /**
     * Check if v1.5 of the custom share sheet is enabled.
     */
    boolean isSharingHubV15Enabled();
}
