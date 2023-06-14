// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link DownloadForegroundServiceImpl}. */
public class DownloadForegroundService extends SplitCompatService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.download.DownloadForegroundServiceImpl";

    public DownloadForegroundService() {
        super(sImplClassName);
    }
}
