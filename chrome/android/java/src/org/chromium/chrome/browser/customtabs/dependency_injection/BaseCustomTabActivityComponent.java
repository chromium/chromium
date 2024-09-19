// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import dagger.Subcomponent;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivityClientConnectionKeeper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.customtabs.CustomTabBottomBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabDownloadObserver;
import org.chromium.chrome.browser.customtabs.CustomTabFeatureOverridesManager;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabSessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabTaskDescriptionHelper;
import org.chromium.chrome.browser.customtabs.ReparentingTaskProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
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
    CurrentPageVerifier resolveCurrentPageVerifier();

    CustomTabActivityClientConnectionKeeper resolveConnectionKeeper();

    CustomTabActivityLifecycleUmaTracker resolveUmaTracker();

    CustomTabActivityNavigationController resolveNavigationController();

    CustomTabActivityTabController resolveTabController();

    CustomTabActivityTabFactory resolveTabFactory();

    CustomTabActivityTabProvider resolveTabProvider();

    CustomTabBottomBarDelegate resolveBottomBarDelegate();

    CustomTabCompositorContentInitializer resolveCompositorContentInitializer();

    CustomTabDelegateFactory resolveTabDelegateFactory();

    CustomTabDownloadObserver resolveDownloadObserver();

    CustomTabIncognitoManager resolveCustomTabIncognitoManager();

    CustomTabIntentHandler resolveIntentHandler();

    CustomTabSessionHandler resolveSessionHandler();

    CustomTabStatusBarColorProvider resolveCustomTabStatusBarColorProvider();

    CustomTabTaskDescriptionHelper resolveTaskDescriptionHelper();

    CustomTabToolbarCoordinator resolveToolbarCoordinator();

    TabObserverRegistrar resolveTabObserverRegistrar();

    TwaFinishHandler resolveTwaFinishHandler();

    Verifier resolveVerifier();

    CustomTabMinimizationManagerHolder resolveCustomTabMinimizationManagerHolder();

    CustomTabFeatureOverridesManager resolveCustomTabFeatureOverridesManager();

    // Webapp & WebAPK only
    WebappActivityCoordinator resolveWebappActivityCoordinator();

    // WebAPK only
    WebApkActivityCoordinator resolveWebApkActivityCoordinator();

    // TWA only
    TrustedWebActivityCoordinator resolveTrustedWebActivityCoordinator();

    // AuthTab only
    AuthTabVerifier resolveAuthTabVerifier();

    // For testing
    CustomTabTabPersistencePolicy resolveTabPersistencePolicy();

    ReparentingTaskProvider resolveReparentingTaskProvider();

    SplashController resolveSplashController();
}
