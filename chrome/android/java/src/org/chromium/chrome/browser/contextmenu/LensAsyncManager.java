// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.lens.LensQueryResult;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.ui.base.WindowAndroid;

// TODO(b/170970926):  Move LensAsyncManager to the private code repository.
/**
 * Manage requests to the Lens SDK which may be asynchronous.
 */
class LensAsyncManager {
    private static final String TAG = "LensAsyncManager";

    private ContextMenuParams mParams;
    private ContextMenuNativeDelegate mNativeDelegate;
    private LensQueryResult mLastCompletedQueryResult;
    private WindowAndroid mWindow;
    private boolean mIsIncognito;
    private String mPageTitle;

    /**
     * Construct a lens async manager.
     * @param params Context menu params used to retrieve additional metadata.
     * @param nativeDelegate {@link ContextMenuNativeDelegate} used to retrieve image bytes.
     * @param window The current window.
     * @param isIncognito Whether the current tab is in incognito mode.
     */
    public LensAsyncManager(ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate,
            WindowAndroid window, boolean isIncognito, String pageTitle) {
        mParams = params;
        mNativeDelegate = nativeDelegate;
        mWindow = window;
        mIsIncognito = isIncognito;
        mPageTitle = pageTitle;
    }

    /**
     * Make a Lens image query for the current render frame.
     * @param replyCallback The function to callback with the query result.
     */
    public void queryImageAsync(Callback<LensQueryResult> replyCallback) {
        Callback<Uri> callback = (uri) -> {
            LensQueryParams lensQueryParams =
                    (new LensQueryParams.Builder())
                            .withImageUri(uri)
                            .withPageUrl(mParams.getPageUrl())
                            .withImageTitleOrAltText(mParams.getTitleText())
                            .withPageTitle(mPageTitle)
                            .build();
            LensController.getInstance().queryImage(lensQueryParams, (lensQueryResult) -> {
                mLastCompletedQueryResult = lensQueryResult;
                replyCallback.onResult(lensQueryResult);
            });
        };
        // Must occur on UI thread.
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.ORIGINAL, callback);
    }

    /**
     * Search with Google Lens with the last completed image query result.
     */
    public void searchWithGoogleLens() {
        Callback<Uri> callback = (uri) -> {
            ShareHelper.shareImageWithGoogleLens(mWindow, uri, mIsIncognito, mParams.getSrcUrl(),
                    mParams.getTitleText(), mLastCompletedQueryResult,
                    /* requiresConfirmation*/ false);
        };
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.PNG, callback);
    }
}