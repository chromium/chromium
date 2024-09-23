// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareParams;

/** Interface to expose sharing to external classes. */
public interface ShareDelegate {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Ensure new values are also added to ShareOrigin in
    // //tools/metrics/histograms/enums.xml.
    @IntDef({
        ShareOrigin.OVERFLOW_MENU,
        ShareOrigin.TOP_TOOLBAR,
        ShareOrigin.CONTEXT_MENU,
        ShareOrigin.WEBSHARE_API,
        ShareOrigin.MOBILE_ACTION_MODE,
        ShareOrigin.EDIT_URL,
        ShareOrigin.TAB_GROUP,
        ShareOrigin.WEBAPP_NOTIFICATION,
        ShareOrigin.FEED,
        ShareOrigin.GOOGLE_BOTTOM_BAR,
        ShareOrigin.CUSTOM_TAB_SHARE_BUTTON,
        ShareOrigin.COUNT
    })
    public @interface ShareOrigin {
        int OVERFLOW_MENU = 0;
        int TOP_TOOLBAR = 1;
        int CONTEXT_MENU = 2;
        int WEBSHARE_API = 3;
        int MOBILE_ACTION_MODE = 4;
        int EDIT_URL = 5;
        int TAB_GROUP = 6;
        int WEBAPP_NOTIFICATION = 7;
        int FEED = 8;
        int GOOGLE_BOTTOM_BAR = 10;
        int CUSTOM_TAB_SHARE_BUTTON = 11;

        // Must be the last one.
        int COUNT = 12;
    }

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

    /** Check if the custom share sheet is enabled. */
    boolean isSharingHubEnabled();
}
