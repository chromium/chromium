// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import android.app.Activity;

import org.chromium.url.GURL;

/**
 * Interface to expose the experimental 'Crow' sharing feature in the App menu
 * footer and Feed to external classes.
 */
public interface CrowButtonDelegate {
    /**
     * @return Whether to show the chip for |url|.
     *
     * @param url URL for current tab where the app menu was launched.
     */
    boolean isEnabledForSite(GURL url);

    /**
     * Launches a custom tab to a server-provided interaction flow.
     * Uses URL defined by the study config.
     *
     * @param currentActivity the current Activity for which the user activated an
     *                        entry point.
     * @param pageUrl URL for the page; passed in rather than derived from currentTab
     *     or WebContents's lastCommittedURL as it was used to construct UI in the caller.
     */
    void launchCustomTab(Activity currentActivity, GURL pageUrl);

    /**
     * @return experiment-configured chip text.
     */
    String getButtonText();
}
