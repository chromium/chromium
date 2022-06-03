// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

/**
 * Receives events from download later dialog.
 */
public interface DownloadLaterDialogController {
    /**
     * Called when the selection changed in the download later dialog.
     * @param choice The selection of the download time in the download later dialog.
     * @param startTime The start time of the download, selected int the date time picker, or -1
     *                  if the user didn't select the time.
     */
    void onDownloadLaterDialogComplete(@DownloadLaterDialogChoice int choice, long startTime);

    /**
     * Called when the user cancels or dismisses the download location dialog.
     */
    void onDownloadLaterDialogCanceled();

    /**
     * Called when the user clicks the edit location text.
     */
    void onEditLocationClicked();
}
