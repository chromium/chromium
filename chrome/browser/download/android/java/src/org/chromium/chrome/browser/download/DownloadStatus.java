// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Status of a download item. */
@IntDef({
    DownloadStatus.IN_PROGRESS,
    DownloadStatus.COMPLETE,
    DownloadStatus.FAILED,
    DownloadStatus.CANCELLED,
    DownloadStatus.INTERRUPTED
})
@Retention(RetentionPolicy.SOURCE)
public @interface DownloadStatus {
    int IN_PROGRESS = 0;
    int COMPLETE = 1;
    int FAILED = 2;
    int CANCELLED = 3;
    int INTERRUPTED = 4;
}
