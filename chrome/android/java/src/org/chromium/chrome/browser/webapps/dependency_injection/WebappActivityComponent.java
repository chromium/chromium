// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.dependency_injection;

import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.webapps.SplashController;
import org.chromium.chrome.browser.webapps.WebappActivityTabController;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.webapps.WebappActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, WebappActivityModule.class})
@ActivityScope
public interface WebappActivityComponent extends ChromeActivityComponent {
    CustomTabActivityNavigationController resolveNavigationController();
    CustomTabCompositorContentInitializer resolveCompositorContentInitializer();
    CustomTabToolbarCoordinator resolveToolbarCoordinator();
    SplashController resolveSplashController();
    TabObserverRegistrar resolveTabObserverRegistrar();
    WebappActivityTabController resolveTabController();
}
