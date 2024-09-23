// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Controls the flow of page info sharing. */
public interface PageInfoSharingController {

    /**
     * Initializes this controller, should be done close to startup to ensure it's available when
     * the user wants to share something.
     */
    void initialize();

    /**
     * Determines if a summary option should be shown in the share sheet for the current tab. This
     * emits metrics, so it should be called once every time the share sheet is shown.
     *
     * @param tab A tab to share.
     * @return True if the current tab supports page info sharing and there's no other tab sharing
     *     sheet open.
     */
    boolean shouldShowInShareSheet(Tab tab);

    /**
     * Starts the process of sharing page info.
     *
     * @param context Android context to create bottom sheet UI.
     * @param bottomSheetController Bottom sheet controller to display sharing UI.
     * @param chromeOptionShareCallback Callback to create a new Android share sheet.
     * @param tab Tab to share.
     */
    void sharePageInfo(
            Context context,
            BottomSheetController bottomSheetController,
            ChromeOptionShareCallback chromeOptionShareCallback,
            Tab tab);

    /**
     * Removes page info from a share sheet.
     *
     * @param chromeOptionShareCallback Callback to create a new Android share sheet.
     * @param tab A tab to share without page info.
     */
    void shareWithoutPageInfo(ChromeOptionShareCallback chromeOptionShareCallback, Tab tab);
}
