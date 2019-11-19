// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_capture;

import org.chromium.chrome.browser.history.HistoryDeletionBridge;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.components.content_capture.ContentCaptureController;

/** History deletion observer that calls ContentCapture methods. */
public class ContentCaptureHistoryDeletionObserver implements HistoryDeletionBridge.Observer {
    /** Observer method when a bit of history is deleted. */
    @Override
    public void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        ContentCaptureController contentCaptureController = ContentCaptureController.getInstance();
        if (contentCaptureController == null) return;

        if (historyDeletionInfo.isTimeRangeForAllTime()
                || (historyDeletionInfo.isTimeRangeValid()
                        && historyDeletionInfo.getTimeRangeBegin()
                                != historyDeletionInfo.getTimeRangeEnd())) {
            contentCaptureController.clearAllContentCaptureData();
        } else {
            String[] deletedURLs = historyDeletionInfo.getDeletedURLs();
            if (deletedURLs.length > 0) {
                contentCaptureController.clearContentCaptureDataForURLs(deletedURLs);
            }
        }
    }
}
