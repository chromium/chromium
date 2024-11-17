// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import dagger.Component;

import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;

import javax.inject.Singleton;

/** Component representing the Singletons in the main process of the application. */
@Component
@Singleton
public interface ChromeAppComponent {
    ChromeActivityComponent createChromeActivityComponent(ChromeActivityCommonsModule module);

    BaseCustomTabActivityComponent createBaseCustomTabActivityComponent(
            ChromeActivityCommonsModule module,
            BaseCustomTabActivityModule baseCustomTabActivityModule);
}
