// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.SPECULATED_URL;

import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that urls are
 * properly loaded in Custom Tabs in different conditions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_WEB_APP_LAUNCH_HANDLER})
@Config(manifest = Config.NONE)
public class CustomTabActivityUrlLoadingTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityTabController mTabController;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabIntentHandler mIntentHandler;

    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private UserPrefsJni mMockUserPrefsJni;

    @Before
    public void setUp() {
        Origin.setOpaqueOriginFactoryForTesting(() -> null);
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        doReturn(mock(PrefService.class)).when(mMockUserPrefsJni).get(any());

        // Ensure the test can read the Autofill pref. Assume it's turned off by default.
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);

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

        mIntentHandler.onNewIntent(createDataProviderForNewIntent());
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    @Test
    public void loadsUrlFromTheLastIntent_IfTwoIntentsArriveBeforeNativeInit() {
        mIntentHandler.onNewIntent(createDataProviderForNewIntent());
        mTabController.setUpInitialTab(null);
        mTabController.finishNativeInitialization();

        verify(env.tabFromFactory, times(1)).loadUrl(any()); // Check that only one call was made.
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent() {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        when(dataProvider.getSession()).thenReturn(env.session);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(OTHER_URL));

        when(dataProvider.getIntent()).thenReturn(intent);
        return dataProvider;
    }
}
