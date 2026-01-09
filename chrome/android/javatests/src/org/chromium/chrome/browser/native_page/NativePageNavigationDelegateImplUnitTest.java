// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

/** Unit tests for {@NativePageNavigationDelegateImpl}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NativePageNavigationDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Profile mProfile;
    @Mock private NativePageHost mHost;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private MultiInstanceManager mMultiInstanceManager;

    private Activity mActivity;
    private NativePageNavigationDelegateImpl mDelegate;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mDelegate =
                new NativePageNavigationDelegateImpl(
                        mActivity, mProfile, mHost, mTabModelSelector, mTab, mMultiInstanceManager);
    }

    @Test
    @SmallTest
    public void testOpenUrl_currentTab() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);

        Tab result = mDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB, urlParams);
        verify(mHost).loadUrl(urlParams, false);
        Assert.assertEquals(mTab, result);
    }

    @Test
    @SmallTest
    public void testOpenUrl_newForegroundTab() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        when(mTabModelSelector.openNewTab(any(), anyInt(), any(), eq(false))).thenReturn(mTab);

        Tab result = mDelegate.openUrl(WindowOpenDisposition.NEW_FOREGROUND_TAB, urlParams);
        verify(mTabModelSelector)
                .openNewTab(
                        urlParams,
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                        mTab,
                        /* incognito= */ false);
        Assert.assertEquals(mTab, result);
    }

    @Test
    @SmallTest
    public void testOpenUrl_newBackgroundTab() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        when(mTabModelSelector.openNewTab(any(), anyInt(), any(), eq(false))).thenReturn(mTab);

        Tab result = mDelegate.openUrl(WindowOpenDisposition.NEW_BACKGROUND_TAB, urlParams);
        verify(mTabModelSelector)
                .openNewTab(
                        urlParams,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        mTab,
                        /* incognito= */ false);
        Assert.assertEquals(mTab, result);
    }

    @Test
    @SmallTest
    public void testOpenUrl_offTheRecord() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        Tab result = mDelegate.openUrl(WindowOpenDisposition.OFF_THE_RECORD, urlParams);
        verify(mHost).loadUrl(urlParams, true);
        Assert.assertNull(result);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOpenUrl_newWindow_incognitoWindowingEnabled() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        when(mHost.getParentId()).thenReturn(1);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        doNothing()
                .when(mMultiInstanceManager)
                .openUrlInOtherWindow(
                        urlParams,
                        1,
                        false,
                        PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);

        Tab result = mDelegate.openUrl(WindowOpenDisposition.NEW_WINDOW, urlParams);
        verify(mMultiInstanceManager)
                .openUrlInOtherWindow(
                        urlParams,
                        1,
                        false,
                        PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);
        Assert.assertNull(result);
    }
}
