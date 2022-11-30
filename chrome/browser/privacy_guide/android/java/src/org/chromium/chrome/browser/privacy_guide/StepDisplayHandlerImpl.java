// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;

/**
 * Computes for each privacy guide step whether it should be displayed or not.
 */
class StepDisplayHandlerImpl implements StepDisplayHandler {
    @Override
    public boolean shouldDisplaySync() {
        // TODO: Check the settings for the SyncFragment.
        return true;
    }

    @Override
    public boolean shouldDisplaySafeBrowsing() {
        return SafeBrowsingBridge.getSafeBrowsingState() != SafeBrowsingState.NO_SAFE_BROWSING;
    }

    @Override
    public boolean shouldDisplayCookies() {
        // TODO: Check the settings for the CookiesFragment.
        return true;
    }
}
