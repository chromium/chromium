// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityClientConnectionKeeper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.customtabs.CustomTabBottomBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.CustomTabSessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabTaskDescriptionHelper;
import org.chromium.chrome.browser.customtabs.CustomTabUmaRecorder;
import org.chromium.chrome.browser.customtabs.ReparentingTaskProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleCoordinator;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleToolbarController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, CustomTabActivityModule.class})
@ActivityScope
public interface CustomTabActivityComponent extends ChromeActivityComponent {
    TrustedWebActivityCoordinator resolveTrustedWebActivityCoordinator();
    DynamicModuleToolbarController resolveDynamicModuleToolbarController();
    DynamicModuleCoordinator resolveDynamicModuleCoordinator();

    CustomTabTaskDescriptionHelper resolveTaskDescriptionHelper();
    CustomTabBottomBarDelegate resolveBottomBarDelegate();
    CustomTabActivityTabController resolveTabController();
    CustomTabActivityTabFactory resolveTabFactory();
    CustomTabActivityLifecycleUmaTracker resolveUmaTracker();
    CustomTabIntentHandler resolveIntentHandler();
    CustomTabActivityNavigationController resolveNavigationController();
    CustomTabActivityTabProvider resolveTabProvider();
    CustomTabStatusBarColorProvider resolveCustomTabStatusBarColorProvider();
    CustomTabToolbarColorController resolveToolbarColorController();
    CustomTabToolbarCoordinator resolveToolbarCoordinator();
    CustomTabUmaRecorder resolveCustomTabUmaRecorder();
    CustomTabCompositorContentInitializer resolveCompositorContentInitializer();
    CustomTabSessionHandler resolveSessionHandler();
    CustomTabActivityClientConnectionKeeper resolveConnectionKeeper();
    TabObserverRegistrar resolveTabObserverRegistrar();
    TwaFinishHandler resolveTwaFinishHandler();
    ImmersiveModeController resolveImmersiveModeController();

    // For testing
    CustomTabTabPersistencePolicy resolveTabPersistencePolicy();
    ReparentingTaskProvider resolveReparentingTaskProvider();
}
