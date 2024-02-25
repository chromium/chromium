// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.content.Context;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Coordinator of the page info bottom sheet.
 *
 * <p>This component shows a bottom sheet to let the add page info to shared links.
 */
public class PageInfoBottomSheetCoordinator {
    private final Context mContext;
    private final PageInfoBottomSheetMediator mMediator;

    /**
     * Creates the coordinator.
     *
     * @param context The context for this component.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     */
    public PageInfoBottomSheetCoordinator(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mMediator =
                new PageInfoBottomSheetMediator(
                        new PageInfoBottomSheet(context), bottomSheetController);
    }

    /** Request to show the bottom sheet. */
    public void requestShowContent() {}

    /** Destroys this component hiding the bottom sheet if needed. */
    public void destroy() {}
}
