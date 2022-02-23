// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatCustomTabsService;

/** See {@link CustomTabsConnectionServiceImpl}. */
public class CustomTabsConnectionService extends SplitCompatCustomTabsService {
    @IdentifierNameString
    private static String sImplClassName =
            "org.chromium.chrome.browser.customtabs.CustomTabsConnectionServiceImpl";

    public CustomTabsConnectionService() {
        super(sImplClassName);
    }
}
