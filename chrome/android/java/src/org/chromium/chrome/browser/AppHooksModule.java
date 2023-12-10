// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import dagger.Module;
import dagger.Provides;

import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;

/**
 * Makes entities provided by AppHooks available for injection with Dagger.
 * TODO(pshmakov): merge this with Chrome's AppHooksImpl.
 */
@Module
public class AppHooksModule {
    /** See {@link ModuleFactoryOverrides} */
    public interface Factory {
        AppHooksModule create();
    }

    @Provides
    public static CustomTabsConnection provideCustomTabsConnection() {
        return CustomTabsConnection.getInstance();
    }

    @Provides
    public ExternalAuthUtils provideExternalAuthUtils() {
        return ExternalAuthUtils.getInstance();
    }

    @Provides
    public MultiWindowUtils provideMultiWindowUtils() {
        return MultiWindowUtils.getInstance();
    }
}
