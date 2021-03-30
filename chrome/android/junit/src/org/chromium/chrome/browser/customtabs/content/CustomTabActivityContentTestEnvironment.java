// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabObserver;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.ReparentingTaskProvider;
import org.chromium.chrome.browser.customtabs.shadows.ShadowExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * A TestRule that sets up the mocks and contains helper methods for JUnit/Robolectric tests scoped
 * to the content layer of Custom Tabs code.
 */
public class CustomTabActivityContentTestEnvironment extends TestWatcher {
    public static final String INITIAL_URL = JUnitTestGURLs.INITIAL_URL;
    public static final String SPECULATED_URL = JUnitTestGURLs.SPECULATED_URL;
    public static final String OTHER_URL = JUnitTestGURLs.EXAMPLE_URL;

    public final Intent intent = new Intent();

    // clang-format off
    @Mock public CustomTabDelegateFactory customTabDelegateFactory;
    @Mock public ChromeActivity activity;
    @Mock public CustomTabsConnection connection;
    @Mock public CustomTabIntentDataProvider intentDataProvider;
    @Mock public TabObserverRegistrar tabObserverRegistrar;
    @Mock public CompositorViewHolder compositorViewHolder;
    @Mock public WarmupManager warmupManager;
    @Mock public CustomTabTabPersistencePolicy tabPersistencePolicy;
    @Mock public CustomTabActivityTabFactory tabFactory;
    @Mock public CustomTabsTabModelOrchestrator tabModelOrchestrator;
    @Mock public CustomTabObserver customTabObserver;
    @Mock public WebContentsFactory webContentsFactory;
    @Mock public ActivityTabProvider activityTabProvider;
    @Mock public ActivityLifecycleDispatcher lifecycleDispatcher;
    @Mock public CustomTabsSessionToken session;
    @Mock public TabModelSelectorImpl tabModelSelector;
    @Mock public TabModel tabModel;
    @Mock public ReparentingTaskProvider reparentingTaskProvider;
    @Mock public ReparentingTask reparentingTask;
    @Mock public CustomTabNavigationEventObserver navigationEventObserver;
    @Mock public CloseButtonNavigator closeButtonNavigator;
    @Mock public ToolbarManager toolbarManager;
    @Mock public ChromeBrowserInitializer browserInitializer;
    @Mock public FullscreenManager fullscreenManager;
    @Mock public StartupTabPreloader startupTabPreloader;
    @Mock public CustomTabIncognitoManager customTabIncognitoManager;
    // clang-format on
    public AsyncTabParamsManager realAsyncTabParamsManager =
            AsyncTabParamsManagerFactory.createAsyncTabParamsManager();

    public final CustomTabActivityTabProvider tabProvider = new CustomTabActivityTabProvider();

    @Captor
    public ArgumentCaptor<ActivityTabObserver> activityTabObserverCaptor;

    // Captures the WebContents with which tabFromFactory is initialized
    @Captor public ArgumentCaptor<WebContents> webContentsCaptor;

    public Tab tabFromFactory;

    public boolean isIncognito;

    @Override
    protected void starting(Description description) {
        MockitoAnnotations.initMocks(this);

        // There are a number of places that call CustomTabsConnection.getInstance(), which would
        // otherwise result in a real CustomTabsConnection being created.
        CustomTabsConnection.setInstanceForTesting(connection);

        tabFromFactory = prepareTab();

        when(intentDataProvider.getIntent()).thenReturn(intent);
        when(intentDataProvider.getSession()).thenReturn(session);
        when(intentDataProvider.getUrlToLoad()).thenReturn(INITIAL_URL);
        when(tabFactory.createTab(webContentsCaptor.capture(), any(), any()))
                .thenReturn(tabFromFactory);
        when(tabFactory.getTabModelOrchestrator()).thenReturn(tabModelOrchestrator);
        when(tabFactory.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelOrchestrator.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelSelector.getModel(anyBoolean())).thenReturn(tabModel);
        when(connection.getSpeculatedUrl(any())).thenReturn(SPECULATED_URL);
        when(browserInitializer.isFullBrowserInitialized()).thenReturn(true);
        // Default setup is toolbarManager doesn't consume back press event.
        when(toolbarManager.back()).thenReturn(false);

        when(startupTabPreloader.takeTabIfMatchingOrDestroy(any(), anyInt())).thenReturn(null);
        when(reparentingTaskProvider.get(any())).thenReturn(reparentingTask);
        doNothing()
                .when(activityTabProvider)
                .addObserverAndTrigger(activityTabObserverCaptor.capture());
    }

    @Override
    protected void finished(Description description) {
        realAsyncTabParamsManager.getAsyncTabParams().clear();
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(false);
    }

    // clang-format off
    public CustomTabActivityTabController createTabController() {
        return new CustomTabActivityTabController(activity, () -> customTabDelegateFactory,
                connection, intentDataProvider, activityTabProvider, tabObserverRegistrar,
                () -> compositorViewHolder, lifecycleDispatcher, warmupManager,
                tabPersistencePolicy, tabFactory, () -> customTabObserver, webContentsFactory,
                navigationEventObserver, tabProvider, startupTabPreloader, reparentingTaskProvider,
                () -> customTabIncognitoManager, () -> realAsyncTabParamsManager,
                () -> activity.getSavedInstanceState());
    }
    // clang-format on

    public CustomTabActivityNavigationController createNavigationController(
            CustomTabActivityTabController tabController) {
        CustomTabActivityNavigationController controller =
                new CustomTabActivityNavigationController(tabController, tabProvider,
                        intentDataProvider, connection,
                        ()
                                -> customTabObserver,
                        closeButtonNavigator, browserInitializer, activity, lifecycleDispatcher,
                        () -> fullscreenManager);
        controller.onToolbarInitialized(toolbarManager);
        return controller;
    }

    public CustomTabIntentHandler createIntentHandler(
            CustomTabActivityNavigationController navigationController) {
        CustomTabIntentHandlingStrategy strategy =
                new DefaultCustomTabIntentHandlingStrategy(tabProvider, navigationController,
                        navigationEventObserver, () -> customTabObserver) {
                    @Override
                    public GURL getGurlForUrl(String url) {
                        return JUnitTestGURLs.getGURL(url);
                    }
                };
        return new CustomTabIntentHandler(tabProvider,
                intentDataProvider, strategy, (intent) -> false, activity);
    }


    public void warmUp() {
        when(connection.hasWarmUpBeenFinished()).thenReturn(true);
    }

    public void changeTab(Tab newTab) {
        when(activityTabProvider.get()).thenReturn(newTab);
        for (ActivityTabObserver observer : activityTabObserverCaptor.getAllValues()) {
            observer.onActivityTabChanged(newTab, false);
        }
    }

    public void saveTab(Tab tab) {
        when(activity.getSavedInstanceState()).thenReturn(new Bundle());
        when(tabModelSelector.getCurrentTab()).thenReturn(tab);
    }

    // Dispatches lifecycle events up to native init.
    public void reachNativeInit(CustomTabActivityTabController tabController) {
        tabController.onPreInflationStartup();
        tabController.onPostInflationStartup();
        tabController.finishNativeInitialization();
    }

    public WebContents prepareTransferredWebcontents() {
        int tabId = 1;
        WebContents webContents = mock(WebContents.class);
        realAsyncTabParamsManager.add(
                tabId, new AsyncTabCreationParams(mock(LoadUrlParams.class), webContents));
        IntentHandler.setTabId(intent, tabId);
        IntentHandler.setForceIntentSenderChromeToTrue(true);
        return webContents;
    }

    public WebContents prepareSpareWebcontents() {
        WebContents webContents = mock(WebContents.class);
        when(warmupManager.takeSpareWebContents(
                     anyBoolean(), anyBoolean(), eq(WarmupManager.FOR_CCT)))
                .thenReturn(webContents);
        return webContents;
    }

    public TabImpl prepareHiddenTab() {
        warmUp();
        TabImpl hiddenTab = prepareTab();
        when(connection.takeHiddenTab(any(), any(), any())).thenReturn(hiddenTab);
        return hiddenTab;
    }

    public TabImpl prepareTab() {
        TabImpl tab = mock(TabImpl.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        WebContents webContents = mock(WebContents.class);
        when(tab.getWebContents()).thenReturn(webContents);
        NavigationController navigationController = mock(NavigationController.class);
        when(webContents.getNavigationController()).thenReturn(navigationController);
        when(tab.isIncognito()).thenAnswer((mock) -> isIncognito);
        return tab;
    }
}
