// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatCustomTabsService;

/** See {@link CustomTabsConnectionServiceImpl}. */
public class CustomTabsConnectionService extends SplitCompatCustomTabsService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.customtabs.CustomTabsConnectionServiceImpl";

    public CustomTabsConnectionService() {
        super(sImplClassName);
    }
}
