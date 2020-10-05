// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_capture;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.history.HistoryDeletionBridge;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.components.content_capture.ContentCaptureController;

/** History deletion observer that calls ContentCapture methods. */
public class ContentCaptureHistoryDeletionObserver implements HistoryDeletionBridge.Observer {
    Supplier<ContentCaptureController> mContentCaptureControllerSupplier;

    public ContentCaptureHistoryDeletionObserver(
            Supplier<ContentCaptureController> contentCaptureControllerSupplier) {
        mContentCaptureControllerSupplier = contentCaptureControllerSupplier;
    }

    /** Observer method when a bit of history is deleted. */
    @Override
    public void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        ContentCaptureController contentCaptureController = mContentCaptureControllerSupplier.get();
        if (contentCaptureController == null) return;

        // A timerange deletion is equivalent to deleting "all" history.
        if (historyDeletionInfo.isTimeRangeForAllTime() || historyDeletionInfo.isTimeRangeValid()) {
            contentCaptureController.clearAllContentCaptureData();
        } else {
            String[] deletedURLs = historyDeletionInfo.getDeletedURLs();
            if (deletedURLs.length > 0) {
                try {
                    contentCaptureController.clearContentCaptureDataForURLs(deletedURLs);
                } catch (RuntimeException e) {
                    throw new RuntimeException("Deleted URLs length: " + deletedURLs.length, e);
                }
            }
        }
    }
}
