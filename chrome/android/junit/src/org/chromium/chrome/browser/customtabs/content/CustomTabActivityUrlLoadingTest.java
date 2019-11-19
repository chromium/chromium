// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.SPECULATED_URL;

import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that urls are
 * properly loaded in Custom Tabs in different conditions.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabActivityUrlLoadingTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    private CustomTabActivityTabController mTabController;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabIntentHandler mIntentHandler;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mTabController = env.createTabController();
        mNavigationController = env.createNavigationController(mTabController);
        mIntentHandler = env.createIntentHandler(mNavigationController);
    }

    @Test
    public void startsLoadingPage_InEarlyCreatedTab() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        verify(env.tabFromFactory).loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));
    }

    @Test
    public void requestsWindowFeature_BeforeAddingContent() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        InOrder inOrder = inOrder(env.activity, env.tabFromFactory);
        inOrder.verify(env.activity).supportRequestWindowFeature(anyInt());
        inOrder.verify(env.tabFromFactory).loadUrl(any());
    }

    @Test
    public void doesntLoadInitialUrlAgain_IfTabChanges() {
        env.reachNativeInit(mTabController);
        clearInvocations(env.tabFromFactory);

        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        verify(newTab, never()).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void loadsUrlInNewTab_IfTabChanges() {
        env.reachNativeInit(mTabController);
        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        clearInvocations(env.tabFromFactory);
        mNavigationController.navigate(OTHER_URL);
        verify(newTab).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadInitialUrl_InRestoredTab() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        env.reachNativeInit(mTabController);
        verify(savedTab, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadUrl_IfEqualsSpeculatedUrl_AndIsFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(SPECULATED_URL);
        env.reachNativeInit(mTabController);
        verify(hiddenTab, never()).loadUrl(any());
    }

    @Test
    public void loadUrl_IfEqualsSpeculatedUrl_ButIsntFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        env.reachNativeInit(mTabController);

        clearInvocations(env.tabFromFactory);
        LoadUrlParams params = new LoadUrlParams(SPECULATED_URL);
        mNavigationController.navigate(params, 0);
        verify(hiddenTab).loadUrl(params);
    }

    @Test
    public void loadsUrlInHiddenTab_IfExists() {
        Tab hiddenTab = env.prepareHiddenTab();
        env.reachNativeInit(mTabController);
        verify(hiddenTab).loadUrl(any());
    }

    @Test
    public void loadsUrlFromNewIntent() {
        env.reachNativeInit(mTabController);
        clearInvocations(env.tabFromFactory);

        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    @Test
    public void loadsUrlFromTheLastIntent_IfTwoIntentsArriveBeforeNativeInit() {
        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        env.reachNativeInit(mTabController);

        verify(env.tabFromFactory, times(1)).loadUrl(any()); // Check that only one call was made.
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent(String url) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getUrlToLoad()).thenReturn(url);
        when(dataProvider.getSession()).thenReturn(env.session);
        when(dataProvider.getIntent()).thenReturn(new Intent().setAction(Intent.ACTION_VIEW));
        return dataProvider;
    }
}
