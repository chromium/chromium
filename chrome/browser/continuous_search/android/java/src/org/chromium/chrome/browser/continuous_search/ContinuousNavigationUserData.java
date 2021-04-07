// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/**
 * Per-tab storage of {@link ContinuousNavigationMetadata}.
 */
public abstract class ContinuousNavigationUserData implements UserData {
    private static final Class<ContinuousNavigationUserData> USER_DATA_KEY =
            ContinuousNavigationUserData.class;

    public static ContinuousNavigationUserData getForTab(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    static void setForTab(Tab tab, ContinuousNavigationUserData userData) {
        tab.getUserDataHost().setUserData(USER_DATA_KEY, userData);
    }

    public abstract void updateData(ContinuousNavigationMetadata metadata, GURL currentUrl);
}
