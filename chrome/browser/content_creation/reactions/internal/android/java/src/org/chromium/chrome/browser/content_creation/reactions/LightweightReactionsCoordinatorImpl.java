// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.share.BaseScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Responsible for reactions main UI and its subcomponents.
 */
public class LightweightReactionsCoordinatorImpl
        extends BaseScreenshotCoordinator implements LightweightReactionsCoordinator {
    private final LightweightReactionsDialog mDialog;
    /**
     * Constructs a new LightweightReactionsCoordinatorImpl which initializes and displays the
     * Lightweight Reactions scene.
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     */
    public LightweightReactionsCoordinatorImpl(Activity activity, Tab tab, String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        super(activity, tab, shareUrl, chromeOptionShareCallback, sheetController);
        mDialog = new LightweightReactionsDialog();
    }

    @Override
    public void showDialog() {
        FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
        mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
    }

    @Override
    protected void handleScreenshot() {
        mDialog.init(mScreenshot);
        showDialog();
    }
}
