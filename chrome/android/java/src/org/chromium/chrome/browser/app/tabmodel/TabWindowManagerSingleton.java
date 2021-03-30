// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.tabmodel.TabWindowManagerFactory;

/**
 * Glue-level singleton instance of {@link TabWindowManager}.
 */
public class TabWindowManagerSingleton {
    private static TabWindowManager sInstance;
    private static TabModelSelectorFactory sSelectorFactoryForTesting;

    /**
     * @return The singleton instance of {@link TabWindowManager}.
     */
    public static TabWindowManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            TabModelSelectorFactory selectorFactory = sSelectorFactoryForTesting == null
                    ? new DefaultTabModelSelectorFactory()
                    : sSelectorFactoryForTesting;
            sInstance = TabWindowManagerFactory.createInstance(
                    selectorFactory, AsyncTabParamsManagerSingleton.getInstance());
        }
        return sInstance;
    }

    /**
     * Allows overriding the default {@link TabModelSelectorFactory} with another one.  Typically
     * for testing.
     * @param factory A {@link TabModelSelectorFactory} instance.
     */
    @VisibleForTesting
    public static void setTabModelSelectorFactoryForTesting(TabModelSelectorFactory factory) {
        assert sInstance == null;
        sSelectorFactoryForTesting = factory;
    }

    @VisibleForTesting
    public static void resetTabModelSelectorFactoryForTesting() {
        sInstance = null;
    }
}