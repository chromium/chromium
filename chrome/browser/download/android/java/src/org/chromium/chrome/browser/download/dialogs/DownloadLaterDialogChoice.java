// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import androidx.annotation.IntDef;

/**
 *  Defines the selection in download later dialog. Used in histograms, don't reuse or remove items.
 *  Keep in sync with DownloadLaterDialogChoice in enums.xml.
 */
@IntDef({DownloadLaterDialogChoice.DOWNLOAD_NOW, DownloadLaterDialogChoice.ON_WIFI,
        DownloadLaterDialogChoice.DOWNLOAD_LATER, DownloadLaterDialogChoice.CANCELLED})
public @interface DownloadLaterDialogChoice {
    /**
     * Download will be started right away.
     */
    int DOWNLOAD_NOW = 0;
    /**
     * Download will be started only on WIFI.
     */
    int ON_WIFI = 1;
    /**
     * Download will be started in the future..
     */
    int DOWNLOAD_LATER = 2;
    /**
     * Download dialog was cancelled.
     */
    int CANCELLED = 3;

    int COUNT = 4;
}
