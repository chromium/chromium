// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.browserservices.ClearDataDialogResultRecorder;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.customtabs.CustomTabsClientFileProcessor;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityModule;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityComponent;
import org.chromium.chrome.browser.webapps.dependency_injection.WebappActivityModule;

import javax.inject.Singleton;

import dagger.Component;

/**
 * Component representing the Singletons in the main process of the application.
 */
@Component(modules = {ChromeAppModule.class, AppHooksModule.class})
@Singleton
public interface ChromeAppComponent {
    ChromeActivityComponent createChromeActivityComponent(ChromeActivityCommonsModule module);

    CustomTabActivityComponent createCustomTabActivityComponent(ChromeActivityCommonsModule module,
            CustomTabActivityModule customTabActivityModule);
    WebappActivityComponent createWebappActivityComponent(
            ChromeActivityCommonsModule module, WebappActivityModule webappActivityModule);

    CustomTabsConnection resolveCustomTabsConnection();
    SharedPreferencesManager resolveSharedPreferencesManager();
    ChromePreferenceManager resolvePreferenceManager();
    ClearDataDialogResultRecorder resolveTwaClearDataDialogRecorder();
    TrustedWebActivityPermissionManager resolveTwaPermissionManager();
    NotificationPermissionUpdater resolveTwaPermissionUpdater();
    TrustedWebActivityClient resolveTrustedWebActivityClient();

    ExternalAuthUtils resolveExternalAuthUtils();

    CustomTabsClientFileProcessor resolveCustomTabsFileProcessor();
    SessionDataHolder resolveSessionDataHolder();
}
