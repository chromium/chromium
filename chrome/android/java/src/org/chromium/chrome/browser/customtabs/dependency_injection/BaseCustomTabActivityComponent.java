// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Subcomponent;

import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabSessionHandler;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;

/**
 * Activity-scoped component associated with {@link
 * org.chromium.chrome.browser.customtabs.CustomTabActivity} and {@link
 * org.chromium.chrome.browser.webapps.WebappActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, BaseCustomTabActivityModule.class})
@ActivityScope
public interface BaseCustomTabActivityComponent extends ChromeActivityComponent {
    CustomTabIncognitoManager resolveCustomTabIncognitoManager();

    CustomTabSessionHandler resolveSessionHandler();
}
