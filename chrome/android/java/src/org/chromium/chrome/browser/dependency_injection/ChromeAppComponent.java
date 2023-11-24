// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import dagger.Component;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.browserservices.ClearDataDialogResultRecorder;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.chrome.browser.customtabs.CustomTabsClientFileProcessor;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.components.externalauth.ExternalAuthUtils;

import javax.inject.Singleton;

/** Component representing the Singletons in the main process of the application. */
@Component(modules = {ChromeAppModule.class, AppHooksModule.class})
@Singleton
public interface ChromeAppComponent {
    ChromeActivityComponent createChromeActivityComponent(ChromeActivityCommonsModule module);

    BaseCustomTabActivityComponent createBaseCustomTabActivityComponent(
            ChromeActivityCommonsModule module,
            BaseCustomTabActivityModule baseCustomTabActivityModule);

    CustomTabsConnection resolveCustomTabsConnection();

    SharedPreferencesManager resolveChromeSharedPreferences();

    ClearDataDialogResultRecorder resolveClearDataDialogResultRecorder();

    InstalledWebappPermissionManager resolvePermissionManager();

    PermissionUpdater resolvePermissionUpdater();

    TrustedWebActivityClient resolveTrustedWebActivityClient();

    ExternalAuthUtils resolveExternalAuthUtils();

    CustomTabsClientFileProcessor resolveCustomTabsFileProcessor();

    SessionDataHolder resolveSessionDataHolder();
}
