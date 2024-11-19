// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Subcomponent;

import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabSessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManagerHolder;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.webapps.WebApkActivityCoordinator;
import org.chromium.chrome.browser.webapps.WebappActivityCoordinator;

/**
 * Activity-scoped component associated with {@link
 * org.chromium.chrome.browser.customtabs.CustomTabActivity} and {@link
 * org.chromium.chrome.browser.webapps.WebappActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, BaseCustomTabActivityModule.class})
@ActivityScope
public interface BaseCustomTabActivityComponent extends ChromeActivityComponent {
    CustomTabActivityNavigationController resolveNavigationController();

    CustomTabActivityTabController resolveTabController();

    CustomTabActivityTabFactory resolveTabFactory();

    CustomTabIncognitoManager resolveCustomTabIncognitoManager();

    CustomTabIntentHandler resolveIntentHandler();

    CustomTabSessionHandler resolveSessionHandler();

    CustomTabToolbarCoordinator resolveToolbarCoordinator();

    CustomTabMinimizationManagerHolder resolveCustomTabMinimizationManagerHolder();

    // Webapp & WebAPK only
    WebappActivityCoordinator resolveWebappActivityCoordinator();

    // WebAPK only
    WebApkActivityCoordinator resolveWebApkActivityCoordinator();

    // TWA only
    TrustedWebActivityCoordinator resolveTrustedWebActivityCoordinator();

    // For testing
    CustomTabTabPersistencePolicy resolveTabPersistencePolicy();
}
