// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.CONTENT_URI;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.SPECULATED_URL;

import android.content.Intent;
import android.net.Uri;
import android.os.Looper;

import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preloading.PreloadingDataBridge;
import org.chromium.chrome.browser.preloading.PreloadingDataBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that urls are
 * properly loaded in Custom Tabs in different conditions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {CustomTabActivityUrlLoadingTest.ShadowOrigin.class})
@Features.EnableFeatures({
    ChromeFeatureList.CCT_EARLY_NAV,
    ChromeFeatureList.CCT_PREWARM_TAB,
    ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER
})
public class CustomTabActivityUrlLoadingTest {
    @Implements(Origin.class)
    public static class ShadowOrigin {
        @Implementation
        public static Origin createOpaqueOrigin() {
            return null;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityTabController mTabController;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabIntentHandler mIntentHandler;

    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock PreloadingDataBridge.Natives mPreloadingDataBridgeMock;

    @Mock WebAppLaunchHandler.Natives mWebAppLaunchHandlerJniMock;

    @Before
    public void setUp() {
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        PreloadingDataBridgeJni.setInstanceForTesting(mPreloadingDataBridgeMock);
        WebAppLaunchHandlerJni.setInstanceForTesting(mWebAppLaunchHandlerJniMock);

        mTabController = env.createTabController();
        mNavigationController = env.createNavigationController(mTabController);
        mIntentHandler = env.createIntentHandler(mNavigationController);
    }

    @Test
    public void startsLoadingPage_InEarlyCreatedTab() {
        env.warmUp();
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(env.tabFromFactory).loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));
    }

    @Test
    public void doesntLoadInitialUrlAgain_IfTabChanges() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);

        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        verify(newTab, never()).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void loadsUrlInNewTab_IfTabChanges() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        clearInvocations(env.tabFromFactory);
        LoadUrlParams params = new LoadUrlParams(OTHER_URL);
        mNavigationController.navigate(params, new Intent());
        verify(newTab).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadInitialUrl_InRestoredTab() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        verify(savedTab, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadUrl_IfEqualsSpeculatedUrl_AndIsFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(SPECULATED_URL);
        when(env.webContents.getLastCommittedUrl()).thenReturn(GURL.emptyGURL());
        mTabController.setUpInitialTab(hiddenTab);
        mTabController.finishNativeInitialization();
        verify(hiddenTab, never()).loadUrl(any());
    }

    @Test
    public void loadUrl_IfEqualsSpeculatedUrl_ButIsntFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        when(env.webContents.getLastCommittedUrl()).thenReturn(GURL.emptyGURL());
        mTabController.setUpInitialTab(hiddenTab);
        mTabController.finishNativeInitialization();

        clearInvocations(env.tabFromFactory);
        LoadUrlParams params = new LoadUrlParams(SPECULATED_URL);
        mNavigationController.navigate(params, new Intent());
        verify(hiddenTab).loadUrl(params);
    }

    @Test
    public void loadsUrlInHiddenTab_IfExists() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.webContents.getLastCommittedUrl()).thenReturn(GURL.emptyGURL());
        mTabController.setUpInitialTab(hiddenTab);
        mTabController.finishNativeInitialization();
        verify(hiddenTab).loadUrl(any());
    }

    @Test
    public void loadsUrlFromNewIntent() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);

        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    @Test
    public void loadsUrlFromTheLastIntent_IfTwoIntentsArriveBeforeNativeInit() {
        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();

        verify(env.tabFromFactory, times(1)).loadUrl(any()); // Check that only one call was made.
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    private void checkLaunchHandler(
            CustomTabIntentDataProvider intentDataProvider,
            int expectedLoadUrlNumber,
            boolean expectedStartNewNavigation) {
        mIntentHandler.onNewIntent(intentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();
        verify(env.tabFromFactory, times(expectedLoadUrlNumber))
                .loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), eq(true), eq(INITIAL_URL), eq(null), eq(new String[0]));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(
                        any(),
                        eq(expectedStartNewNavigation),
                        eq(OTHER_URL),
                        eq(null),
                        eq(new String[0]));
    }

    @Test
    public void navigateExistingClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider =
                createDataProviderForNewIntent(
                        OTHER_URL, LaunchHandlerClientMode.NAVIGATE_EXISTING);

        checkLaunchHandler(intentDataProvider, 1, true);
    }

    @Test
    public void focusExistingClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider =
                createDataProviderForNewIntent(OTHER_URL, LaunchHandlerClientMode.FOCUS_EXISTING);

        checkLaunchHandler(intentDataProvider, 0, false);
    }

    @Test
    public void autoClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider =
                createDataProviderForNewIntent(OTHER_URL, LaunchHandlerClientMode.AUTO);

        // The user agent(browser) decides what works best for the platform. Currently it's
        // navigate-existing.
        checkLaunchHandler(intentDataProvider, 1, true);
    }

    @Test
    public void wrongClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider =
                createDataProviderForNewIntent(OTHER_URL, 98);

        // Fallback to auto mode as described in the specification.
        checkLaunchHandler(intentDataProvider, 1, true);
    }

    @Test
    public void navigateNewClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider =
                createDataProviderForNewIntent(OTHER_URL, LaunchHandlerClientMode.NAVIGATE_NEW);

        // Treated by IntentHandler as a wrong mode because this mode should be handled earlier by
        // LaunchIntentDispatcher because it require launching of a new task.
        checkLaunchHandler(intentDataProvider, 1, true);
    }

    @Test
    public void noClientMode() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider = createDataProviderForNewIntent(OTHER_URL);

        // According to the specification if not specified, the default client_mode value is auto.
        checkLaunchHandler(intentDataProvider, 1, true);
    }

    private FileHandlingData createFileHandlingData() {
        ArrayList<Uri> sampleList = new ArrayList<>(Arrays.asList(Uri.parse(CONTENT_URI)));
        return new FileHandlingData(sampleList);
    }

    @Test
    public void checkFileHandling() {
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();
        clearInvocations(env.tabFromFactory);
        CustomTabIntentDataProvider intentDataProvider = createDataProviderForNewIntent(OTHER_URL);
        when(intentDataProvider.getFileHandlingData()).thenReturn(createFileHandlingData());

        mIntentHandler.onNewIntent(intentDataProvider);
        shadowOf(Looper.getMainLooper()).idle();
        verify(env.tabFromFactory, times(1))
                .loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(any(), eq(true), eq(INITIAL_URL), eq(null), eq(new String[0]));
        verify(mWebAppLaunchHandlerJniMock, times(1))
                .notifyLaunchQueue(
                        any(), eq(true), eq(OTHER_URL), eq(null), eq(new String[] {CONTENT_URI}));
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent(
            String url, @LaunchHandlerClientMode.ClientMode int clientMode) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getUrlToLoad()).thenReturn(url);
        when(dataProvider.getSession()).thenReturn(env.session);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        when(dataProvider.getLaunchHandlerClientMode()).thenReturn(clientMode);

        when(dataProvider.getIntent()).thenReturn(intent);
        return dataProvider;
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent(String url) {
        return createDataProviderForNewIntent(url, LaunchHandlerClientMode.AUTO);
    }
}
