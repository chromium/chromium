// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.ClientAppDataRegister;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.TabObserverRegistrar;

import dagger.Module;
import dagger.Provides;

/**
 * Module for custom tab specific bindings.
 */
@Module
public class CustomTabActivityModule {
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;

    public CustomTabActivityModule(CustomTabIntentDataProvider intentDataProvider,
            TabObserverRegistrar tabObserverRegistrar) {
        mIntentDataProvider = intentDataProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
    }

    @Provides
    public CustomTabIntentDataProvider provideIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public ClientAppDataRegister provideClientAppDataRegister() {
        return new ClientAppDataRegister();
    }

    @Provides
    public TabObserverRegistrar provideTabObserverRegistrar() {
        return mTabObserverRegistrar;
    }
}
