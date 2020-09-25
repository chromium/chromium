// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;

/**
 * Manage requests to the Lens SDK which may be asynchronous.
 */
class LensAsyncManager {
    private static final String TAG = "LensAsyncManager";

    private ContextMenuParams mParams;
    private ContextMenuPopulator mPopulator;

    /**
     * Construct a lens async manager.
     * @param params Context menu params used to retrieve additional metadata.
     * @param populator A populator reference used to retrieve image bytes.
     */
    public LensAsyncManager(ContextMenuParams params, ContextMenuPopulator populator) {
        mParams = params;
        mPopulator = populator;
    }

    /**
     * Make a lens classification call for the current render frame.
     * @param replyCallback The function to callback with the classification.
     */
    public void classifyImageAsync(Callback<Boolean> replyCallback) {
        // Must occur on UI thread.
        mPopulator.retrieveImage(ContextMenuImageFormat.ORIGINAL, (uri) -> {
            LensController.getInstance().classifyImage(uri,
                    mParams.getPageUrl(),
                    mParams.getTitleText(),
                    (isClassificationSuccessful) -> {
                        replyCallback.onResult(isClassificationSuccessful);
                    });
        });
    }
}