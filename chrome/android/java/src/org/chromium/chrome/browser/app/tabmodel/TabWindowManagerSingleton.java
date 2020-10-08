// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;

/**
 * Glue-level singleton instance of {@link TabWindowManager}.
 */
public class TabWindowManagerSingleton {
    private static TabWindowManager sInstance;

    /**
     * @return The singleton instance of {@link TabWindowManager}.
     */
    public static TabWindowManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new TabWindowManager(new DefaultTabModelSelectorFactory(),
                    AsyncTabParamsManagerSingleton.getInstance());
        }
        return sInstance;
    }
}