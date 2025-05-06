// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.View;

import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.CustomTabsTabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.content.WebContentsFactoryJni;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator;
import org.chromium.chrome.browser.customtabs.CustomTabAuthUrlHeuristics;
import org.chromium.chrome.browser.customtabs.CustomTabAuthUrlHeuristicsJni;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNavigationEventObserver;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManagerHolder;
import org.chromium.chrome.browser.customtabs.shadows.ShadowExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preloading.PreloadingDataBridge;
import org.chromium.chrome.browser.preloading.PreloadingDataBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.url.JUnitTestGURLs;

/**
 * A TestRule that sets up the mocks and contains helper methods for JUnit/Robolectric tests scoped
 * to the content layer of Custom Tabs code.
 */
public class CustomTabActivityContentTestEnvironment extends TestWatcher {
    public static final String INITIAL_URL = JUnitTestGURLs.INITIAL_URL.getSpec();
    public static final String SPECULATED_URL = "https://speculated.com";
    public static final String OTHER_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    public static final String CONTENT_URI = "content://com.a.b.c/a";

    public final Intent mIntent = new Intent();

    @Mock public BaseCustomTabActivity activity;
    @Mock public CustomTabDelegateFactory customTabDelegateFactory;
    @Mock public CustomTabsConnection connection;
    @Mock public ColorProvider colorProvider;
    @Mock public CustomTabIntentDataProvider intentDataProvider;
    @Mock public TabObserverRegistrar tabObserverRegistrar;
    @Mock public CompositorViewHolder compositorViewHolder;
    @Mock public WarmupManager warmupManager;
    @Mock public CustomTabTabPersistencePolicy tabPersistencePolicy;
    @Mock public CustomTabActivityTabFactory tabFactory;
    @Mock public CustomTabsTabModelOrchestrator tabModelOrchestrator;
    @Mock public CustomTabObserver customTabObserver;
    @Mock public ActivityTabProvider activityTabProvider;
    @Mock public ActivityLifecycleDispatcher lifecycleDispatcher;
    @Mock public SessionHolder<CustomTabsSessionToken> session;
    @Mock public TabModelSelectorImpl tabModelSelector;
    @Mock public TabModel tabModel;
    @Mock public ReparentingTask reparentingTask;
    @Mock public CustomTabNavigationEventObserver navigationEventObserver;
    @Mock public CloseButtonNavigator closeButtonNavigator;
    @Mock public ToolbarManager toolbarManager;
    @Mock public ChromeBrowserInitializer browserInitializer;
    @Mock public TabModelInitializer tabModelInitializer;
    @Mock public MockWebContents webContents;
    @Mock public CustomTabMinimizationManagerHolder mMinimizationManagerHolder;
    @Mock public ProfileProvider profileProvider;
    @Mock public CipherFactory cipherFactory;
    @Mock public PowerManager powerManager;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock CustomTabAuthUrlHeuristics.Natives mCustomTabAuthUrlHeuristicsJniMock;
    @Mock WebContentsFactory.Natives mWebContentsFactoryJni;
    @Mock public Verifier verifier;
    @Mock public CurrentPageVerifier currentPageVerifier;
    @Mock PreloadingDataBridge.Natives mPreloadingDataBridgeMock;
    @Mock WebAppLaunchHandler.Natives mWebAppLaunchHandlerJniMock;

    public final CustomTabActivityTabProvider tabProvider =
            new CustomTabActivityTabProvider(SPECULATED_URL);

    @Captor public ArgumentCaptor<Callback<Tab>> activityTabObserverCaptor;

    // Captures the WebContents with which tabFromFactory is initialized
    @Captor public ArgumentCaptor<WebContents> webContentsCaptor;

    public Tab tabFromFactory;

    public boolean isOffTheRecord;

    @Override
    protected void starting(Description description) {
        MockitoAnnotations.initMocks(this);

        CustomTabsConnection.setInstanceForTesting(connection);
        ChromeBrowserInitializer.setForTesting(browserInitializer);
        WarmupManager.setInstanceForTesting(warmupManager);
        CustomTabAuthUrlHeuristicsJni.setInstanceForTesting(mCustomTabAuthUrlHeuristicsJniMock);
        WebContentsFactoryJni.setInstanceForTesting(mWebContentsFactoryJni);
        PreloadingDataBridgeJni.setInstanceForTesting(mPreloadingDataBridgeMock);
        WebAppLaunchHandlerJni.setInstanceForTesting(mWebAppLaunchHandlerJniMock);

        tabFromFactory = prepareTab();

        when(intentDataProvider.getIntent()).thenReturn(mIntent);
        when(intentDataProvider.getSession()).thenReturn(session);
        when(intentDataProvider.getUrlToLoad()).thenReturn(INITIAL_URL);
        when(intentDataProvider.getActivityType()).thenReturn(ActivityType.CUSTOM_TAB);
        when(tabFactory.createTab(webContentsCaptor.capture(), any(), any()))
                .thenReturn(tabFromFactory);
        when(tabFactory.getTabModelOrchestrator()).thenReturn(tabModelOrchestrator);
        when(tabFactory.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelOrchestrator.getTabModelSelector()).thenReturn(tabModelSelector);
        when(tabModelSelector.getModel(anyBoolean())).thenReturn(tabModel);
        when(tabModelSelector.getCurrentModel()).thenReturn(tabModel);
        when(browserInitializer.isFullBrowserInitialized()).thenReturn(true);
        // Default setup is toolbarManager doesn't consume back press event.
        when(toolbarManager.back()).thenReturn(false);

        when(activityTabProvider.addObserver(activityTabObserverCaptor.capture())).thenReturn(null);
        when(intentDataProvider.getColorProvider()).thenReturn(colorProvider);

        when(activity.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);
        when(powerManager.isInteractive()).thenReturn(true);

        when(profileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(profileProvider.getOffTheRecordProfile(eq(true))).thenReturn(mIncognitoProfile);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(verifier.verify(any())).thenReturn(Promise.fulfilled(true));
        when(currentPageVerifier.getState())
                .thenReturn(
                        new CurrentPageVerifier.VerificationState(
                                "", "", CurrentPageVerifier.VerificationStatus.SUCCESS));
    }

    @Override
    protected void finished(Description description) {
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
        ShadowExternalNavigationDelegateImpl.setWillChromeHandleIntent(false);
    }

    public CustomTabActivityTabController createTabController() {
        OneshotSupplierImpl<ProfileProvider> profileProviderSupplier = new OneshotSupplierImpl<>();
        profileProviderSupplier.set(profileProvider);

        return new CustomTabActivityTabController(
                activity,
                profileProviderSupplier,
                customTabDelegateFactory,
                intentDataProvider,
                tabObserverRegistrar,
                () -> compositorViewHolder,
                tabPersistencePolicy,
                tabFactory,
                customTabObserver,
                navigationEventObserver,
                activityTabProvider,
                tabProvider,
                () -> activity.getSavedInstanceState(),
                activity.getWindowAndroid(),
                tabModelInitializer,
                cipherFactory,
                lifecycleDispatcher);
    }

    public CustomTabActivityNavigationController createNavigationController(
            CustomTabActivityTabController tabController) {
        CustomTabActivityNavigationController controller =
                new CustomTabActivityNavigationController(
                        tabController,
                        tabProvider,
                        intentDataProvider,
                        customTabObserver,
                        closeButtonNavigator,
                        activity,
                        lifecycleDispatcher);
        return controller;
    }

    public CustomTabIntentHandler createIntentHandler(
            CustomTabActivityNavigationController navigationController) {
        return new CustomTabIntentHandler(
                tabProvider,
                intentDataProvider,
                new DefaultCustomTabIntentHandlingStrategy(
                        tabProvider,
                        navigationController,
                        customTabObserver,
                        verifier,
                        currentPageVerifier,
                        activity),
                activity,
                mMinimizationManagerHolder);
    }

    public void warmUp() {
        when(connection.hasWarmUpBeenFinished()).thenReturn(true);
    }

    public void changeTab(Tab newTab) {
        when(activityTabProvider.get()).thenReturn(newTab);
        when(tabModel.getCount()).thenReturn(1);
        for (Callback<Tab> observer : activityTabObserverCaptor.getAllValues()) {
            observer.onResult(newTab);
        }
    }

    public void saveTab(Tab tab) {
        when(activity.getSavedInstanceState()).thenReturn(new Bundle());
        when(tabModelSelector.getCurrentTab()).thenReturn(tab);
    }

    public WebContents prepareTransferredWebcontents() {
        int tabId = 1;
        WebContents webContents = mock(WebContents.class);
        AsyncTabParamsManagerSingleton.getInstance()
                .add(tabId, new AsyncTabCreationParams(mock(LoadUrlParams.class), webContents));
        IntentHandler.setTabId(mIntent, tabId);
        IntentUtils.setForceIsTrustedIntentForTesting(true);
        return webContents;
    }

    public WebContents prepareSpareWebcontents() {
        WebContents webContents = mock(WebContents.class);
        when(warmupManager.takeSpareWebContents(
                        /* incognito= */ anyBoolean(),
                        /* initiallyHidden= */ anyBoolean(),
                        /* targetsNetwork= */ anyBoolean()))
                .thenReturn(webContents);
        return webContents;
    }

    public Tab prepareHiddenTab() {
        warmUp();
        Tab hiddenTab = prepareTab();
        return hiddenTab;
    }

    public Tab prepareTransferredTab() {
        int tabId = 1;
        Tab tab = prepareTab();
        when(tab.getId()).thenReturn(tabId);
        AsyncTabParamsManagerSingleton.getInstance()
                .add(tabId, new TabReparentingParams(tab, null));
        IntentHandler.setTabId(mIntent, tabId);
        IntentUtils.setForceIsTrustedIntentForTesting(true);
        return tab;
    }

    public Tab prepareTab() {
        Tab tab = mock(Tab.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        when(tab.getWebContents()).thenReturn(webContents);
        NavigationController navigationController = mock(NavigationController.class);
        when(webContents.getNavigationController()).thenReturn(navigationController);
        when(tab.isIncognito()).thenAnswer((mock) -> isOffTheRecord);
        when(tab.isOffTheRecord()).thenAnswer((mock) -> isOffTheRecord);
        when(tab.isIncognitoBranded()).thenAnswer((mock) -> isOffTheRecord);
        when(intentDataProvider.isOffTheRecord()).thenReturn(isOffTheRecord);
        tab.getUserDataHost().setUserData(ReparentingTask.class, reparentingTask);
        return tab;
    }
}
