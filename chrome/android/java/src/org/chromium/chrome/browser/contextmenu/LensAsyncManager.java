// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;

/**
 * Manage requests to the Lens SDK which may be asynchronous.
 */
class LensAsyncManager {
    private static final String TAG = "LensAsyncManager";

    private ContextMenuParams mParams;
    private ContextMenuNativeDelegate mNativeDelegate;

    /**
     * Construct a lens async manager.
     * @param params Context menu params used to retrieve additional metadata.
     * @param nativeDelegate {@link ContextMenuNativeDelegate} used to retrieve image bytes.
     */
    public LensAsyncManager(ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate) {
        mParams = params;
        mNativeDelegate = nativeDelegate;
    }

    /**
     * Make a lens classification call for the current render frame.
     * @param replyCallback The function to callback with the classification.
     */
    public void classifyImageAsync(Callback<Boolean> replyCallback) {
        Callback<Uri> callback = (uri)
                -> LensController.getInstance().classifyImage(uri, mParams.getPageUrl(),
                        mParams.getTitleText(), (isClassificationSuccessful) -> {
                            replyCallback.onResult(isClassificationSuccessful);
                        });
        // Must occur on UI thread.
        mNativeDelegate.retrieveImageForShare(ContextMenuImageFormat.ORIGINAL, callback);
    }
}