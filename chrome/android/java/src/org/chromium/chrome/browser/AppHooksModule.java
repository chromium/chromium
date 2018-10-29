// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.customtabs.CustomTabsConnection;

import dagger.Module;
import dagger.Provides;

/**
 * Makes entities provided by AppHooks available for injection with Dagger.
 * TODO(pshmakov): merge this with Chrome's AppHooksImpl.
 */
@Module
public class AppHooksModule {
    @Provides
    public static CustomTabsConnection provideCustomTabsConnection() {
        return CustomTabsConnection.getInstance();
    }
}
