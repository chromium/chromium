// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory;

/** Holds a singleton {@link AsyncTabParamsManager} */
public class AsyncTabParamsManagerSingleton {
    /** Singleton instance. */
    private static final AsyncTabParamsManager INSTANCE =
            AsyncTabParamsManagerFactory.createAsyncTabParamsManager();

    /** Get the singleton instance of {@link AsyncTabParamsManager}. */
    public static AsyncTabParamsManager getInstance() {
        return INSTANCE;
    }
}
