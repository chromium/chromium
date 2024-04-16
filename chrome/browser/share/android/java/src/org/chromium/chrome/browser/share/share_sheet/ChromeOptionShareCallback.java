// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.components.browser_ui.share.ShareParams;

/**
 * An interface to help other chrome features surface share sheet APIs.
 *
 * <p>TODO(crbug.com/40100930) This class can become the Public API of ShareSheetCoordinator, and
 * ShareSheetCoordinator can be rewritten as ShareSheetCoordinatorImpl.
 */
public interface ChromeOptionShareCallback {
    /**
     * Used to show only the bottom bar of the share sheet
     * @param params The share parameters.
     * @param chromeShareExtras The extras not contained in {@code params}.
     */
    public void showThirdPartyShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime);

    /**
     * Used to show the share sheet
     * @param params The share parameters.
     * @param chromeShareExtras The extras not contained in {@code params}.
     * @param shareStartTime The start time of the current share.
     */
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime);
}
