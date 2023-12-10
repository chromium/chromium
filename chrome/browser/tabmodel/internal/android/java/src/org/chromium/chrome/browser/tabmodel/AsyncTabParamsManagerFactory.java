// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** Factory for creating {@link AsyncTabParamsManager}. */
public class AsyncTabParamsManagerFactory {
    /**
     * @return New instance of {@link AsyncTabParamsManagerImpl}.
     */
    public static AsyncTabParamsManager createAsyncTabParamsManager() {
        return new AsyncTabParamsManagerImpl();
    }
}
