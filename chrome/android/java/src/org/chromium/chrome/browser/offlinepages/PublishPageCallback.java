// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * This callback will save the state we need when the JNI call is done, and start the next stage of
 * processing for sharing.
 */
public class PublishPageCallback implements Callback<String> {
    private Callback<ShareParams> mShareCallback;
    OfflinePageItem mPage;
    private WindowAndroid mWindow;

    /** Create a callback for use when page publishing is completed. */
    public PublishPageCallback(
            WindowAndroid window, OfflinePageItem page, Callback<ShareParams> shareCallback) {
        mWindow = window;
        mPage = page;
        mShareCallback = shareCallback;
    }

    /** Report results of publishing. */
    @Override
    @CalledByNative
    public void onResult(String newFilePath) {
        OfflinePageItem page = null;
        // If the sharing failed, the file path will be empty.  We'll call the share callback
        // with a null page to indicate failure.
        if (!newFilePath.isEmpty()) {
            // Make a new OfflinePageItem with the new path.
            page =
                    new OfflinePageItem(
                            mPage.getUrl(),
                            mPage.getOfflineId(),
                            mPage.getClientId().getNamespace(),
                            mPage.getClientId().getId(),
                            mPage.getTitle(),
                            newFilePath,
                            mPage.getFileSize(),
                            mPage.getCreationTimeMs(),
                            mPage.getAccessCount(),
                            mPage.getLastAccessTimeMs(),
                            mPage.getRequestOrigin());
        }

        OfflinePageUtils.publishCompleted(page, mWindow, mShareCallback);
    }
}
