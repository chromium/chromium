// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link DownloadForegroundServiceImpl}. */
public class DownloadForegroundService extends SplitCompatService {
    // TODO(crbug.com/1126301):  Use R8's -identifiernamestring to verify this and other
    // SplitCompatService subclasses.
    public DownloadForegroundService() {
        super("org.chromium.chrome.browser.download.DownloadForegroundServiceImpl");
    }
}
