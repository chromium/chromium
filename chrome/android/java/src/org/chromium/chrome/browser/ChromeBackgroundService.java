// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatGcmTaskService;

/** See {@link ChromeBackgroundServiceImpl}. */
public class ChromeBackgroundService extends SplitCompatGcmTaskService {
    @IdentifierNameString
    private static final String IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.ChromeBackgroundServiceImpl";

    public ChromeBackgroundService() {
        super(IMPL_CLASS_NAME);
    }
}
