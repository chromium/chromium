// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

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
     * @return experiment-configured chip text.
     */
    String getButtonText();
}
