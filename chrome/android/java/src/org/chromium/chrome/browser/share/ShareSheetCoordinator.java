// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Coordinator for displaying the share sheet.
 */
public class ShareSheetCoordinator {
    private ShareSheetMediator mMediator;

    /**
     * Constructs a new ShareSheetCoordinator.
     */
    public ShareSheetCoordinator(BottomSheetController controller) {
        mMediator = new ShareSheetMediator(new ShareSheetMediator.ShareSheetDelegate(), controller);
    }

    /**
     * Triggered when the share menu item is selected.
     * This creates and shows a share intent picker dialog or starts a share intent directly.
     * @param activity The activity that triggered this share action.
     * @param tab The tab containing the content to be shared.
     * @param shareDirectly Whether it should share directly with the activity that was most
     *         recently used to share.
     * @param isIncognito Whether currentTab is incognito.
     */
    public void onShareSelected(
            Activity activity, Tab currentTab, boolean shareDirectly, boolean isIncognito) {
        mMediator.onShareSelected(activity, currentTab, shareDirectly, isIncognito);
    }

    /**
     * Triggers a share based on the provided {@link ShareParams}.
     * @param params The container holding the share parameters.
     */
    public void share(ShareParams params) {
        mMediator.share(params);
    }

    @VisibleForTesting
    public static void setScreenshotCaptureSkippedForTesting(boolean value) {
        ShareSheetMediator.setScreenshotCaptureSkippedForTesting(value);
    }
}
