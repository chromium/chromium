// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatGcmTaskService;

/** See {@link ChromeBackgroundServiceImpl}. */
public class ChromeBackgroundService extends SplitCompatGcmTaskService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.ChromeBackgroundServiceImpl";

    public ChromeBackgroundService() {
        super(sImplClassName);
    }
}
