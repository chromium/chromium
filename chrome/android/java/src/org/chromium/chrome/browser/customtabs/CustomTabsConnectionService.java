// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.base.SplitCompatCustomTabsService;
import org.chromium.chrome.browser.base.SplitCompatUtils;

/** See {@link CustomTabsConnectionServiceImpl}. */
public class CustomTabsConnectionService extends SplitCompatCustomTabsService {
    public CustomTabsConnectionService() {
        super(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.customtabs.CustomTabsConnectionServiceImpl"));
    }
}
