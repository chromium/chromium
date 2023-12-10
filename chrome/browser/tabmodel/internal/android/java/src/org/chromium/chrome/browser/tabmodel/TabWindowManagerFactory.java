// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** Factory for creating {@link TabWindowManager}. */
public class TabWindowManagerFactory {
    /**
     * @return New instance of {@link TabWindowManagerImpl}.
     */
    public static TabWindowManager createInstance(
            TabModelSelectorFactory selectorFactory,
            AsyncTabParamsManager asyncTabParamsManager,
            int maxSelectors) {
        return new TabWindowManagerImpl(selectorFactory, asyncTabParamsManager, maxSelectors);
    }
}
