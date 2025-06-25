// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.build.annotations.NullMarked;

/** Receives events from download location dialog. */
@NullMarked
public interface DownloadLocationDialogController {
    /**
     * Called when the download location selection flow finished.
     *
     * @param returnedPath The download file path selected.
     * @param didUserConfirm Whether the result was actively confirmed by user action.
     */
    void onDownloadLocationDialogComplete(String returnedPath, boolean didUserConfirm);

    /** Called when the user cancel or dismiss the download location dialog. */
    void onDownloadLocationDialogCanceled();
}
