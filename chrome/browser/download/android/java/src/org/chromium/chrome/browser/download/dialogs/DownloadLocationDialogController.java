// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

/** Receives events from download location dialog. */
public interface DownloadLocationDialogController {
    /**
     * Called when the user finished download location selection flow.
     * @param returnedPath The download file path picked by the user.
     */
    void onDownloadLocationDialogComplete(String returnedPath);

    /** Called when the user cancel or dismiss the download location dialog. */
    void onDownloadLocationDialogCanceled();
}
