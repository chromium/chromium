// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabwindow;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.DefaultTabModelSelectorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManagerFactory;

/** Glue-level singleton instance of {@link TabWindowManager}. */
@NullMarked
public class TabWindowManagerSingleton {
    private static @Nullable TabWindowManager sInstance;
    private static @Nullable TabModelSelectorFactory sSelectorFactoryForTesting;

    /** Returns the singleton instance of {@link TabWindowManager}. */
    public static TabWindowManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            int maxSelectors = MultiWindowUtils.getMaxInstances();
            TabModelSelectorFactory selectorFactory =
                    sSelectorFactoryForTesting == null
                            ? new DefaultTabModelSelectorFactory()
                            : sSelectorFactoryForTesting;
            sInstance =
                    TabWindowManagerFactory.createInstance(
                            selectorFactory,
                            AsyncTabParamsManagerSingleton.getInstance(),
                            maxSelectors);
        }
        return sInstance;
    }

    /**
     * Allows overriding the default {@link TabModelSelectorFactory} with another one. Typically for
     * testing.
     *
     * @param factory A {@link TabModelSelectorFactory} instance.
     */
    public static void setTabModelSelectorFactoryForTesting(TabModelSelectorFactory factory) {
        assert sInstance == null;
        sSelectorFactoryForTesting = factory;
    }

    public static void setTabWindowManagerForTesting(TabWindowManager manager) {
        sInstance = manager;
        ResettersForTesting.register(
                TabWindowManagerSingleton::resetTabModelSelectorFactoryForTesting);
    }

    public static void resetTabModelSelectorFactoryForTesting() {
        sInstance = null;
        sSelectorFactoryForTesting = null;
    }
}
