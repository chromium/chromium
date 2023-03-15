// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.base.ThreadUtils;

/**
 * A factory interface for building a RestoreTabsController instance.
 */
public class RestoreTabsControllerFactory {
    private static RestoreTabsControllerImpl sInstance;

    /**
     * @return The singleton instance of RestoreTabsControllerImpl.
     */
    public static RestoreTabsControllerImpl getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new RestoreTabsControllerImpl();
        }
        return sInstance;
    }
}