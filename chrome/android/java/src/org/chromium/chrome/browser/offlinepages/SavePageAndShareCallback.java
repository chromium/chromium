// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.ui.base.WindowAndroid;

/**
 * This callback will save get the saved page during live page sharing and share the page if saving
 * process succeeds.
 */
public class SavePageAndShareCallback implements OfflinePageBridge.SavePageCallback {
    private WindowAndroid mWindow;
    private Callback<ShareParams> mShareCallback;
    private OfflinePageBridge mBridge;

    public SavePageAndShareCallback(WindowAndroid window, final Callback<ShareParams> shareCallback,
            OfflinePageBridge bridge) {
        mWindow = window;
        mShareCallback = shareCallback;
        mBridge = bridge;
    }

    @Override
    public void onSavePageDone(int savePageResult, String url, long offlineId) {
        if (savePageResult != SavePageResult.SUCCESS) {
            // If the page is not saved, skip the sharing part.
            return;
        }
        mBridge.getPageByOfflineId(offlineId, new Callback<OfflinePageItem>() {
            @Override
            public void onResult(OfflinePageItem page) {
                OfflinePageUtils.sharePublishedPage(page, mWindow, mShareCallback);
            }
        });
    }
}