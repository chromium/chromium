// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** Unit tests for the {@link TabCreationDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCreationDelegateImplUnitTest {
    private static final GURL TEST_URL = new GURL("https://example.com");
    private static final GURL TEST_URL2 = new GURL("https://example2.com");
    private static final String TEST_TITLE = "Some title";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab1;
    @Mock private TabCreator mTabCreator;
    @Mock private NavigationTracker mNavigationTracker;
    @Mock private Origin mOpaqueOrigin;
    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    private TabCreationDelegateImpl mTabCreationDelegate;

    @Before
    public void setUp() {
        Mockito.doReturn(1).when(mTab1).getId();
        Mockito.doReturn(TEST_URL2).when(mTab1).getUrl();
        Origin.setOpaqueOriginFactoryForTesting(() -> mOpaqueOrigin);
    }

    private void assertSyncLoadUrlParams(LoadUrlParams params) {
        assertTrue(params.getIsRendererInitiated());
        assertEquals(mOpaqueOrigin, params.getInitiatorOrigin());
    }

    @Test
    public void testCreateTabInBackground() {
        mTabCreationDelegate = new TabCreationDelegateImpl(mTabCreator, mNavigationTracker);
        mTabCreationDelegate.createBackgroundTab(TEST_URL, TEST_TITLE, null, 2);
        verify(mTabCreator)
                .createNewTab(
                        mLoadUrlParamsCaptor.capture(),
                        eq(TEST_TITLE),
                        eq(TabLaunchType.FROM_SYNC_BACKGROUND),
                        eq(null),
                        eq(2));
        assertSyncLoadUrlParams(mLoadUrlParamsCaptor.getValue());
    }

    @Test
    public void testNavigateUrlDeferred() {
        mTabCreationDelegate = new TabCreationDelegateImpl(mTabCreator, mNavigationTracker);
        mTabCreationDelegate.navigateToUrl(
                mTab1, TEST_URL, TEST_TITLE, /* isForegroundTab= */ false);
        verify(mTab1)
                .discardAndAppendPendingNavigation(mLoadUrlParamsCaptor.capture(), eq(TEST_TITLE));
        assertSyncLoadUrlParams(mLoadUrlParamsCaptor.getValue());
    }

    @Test
    public void testNavigateUrlInCurrentTab() {
        mTabCreationDelegate = new TabCreationDelegateImpl(mTabCreator, mNavigationTracker);
        mTabCreationDelegate.navigateToUrl(
                mTab1, TEST_URL, TEST_TITLE, /* isForegroundTab= */ true);
        verify(mTab1).loadUrl(mLoadUrlParamsCaptor.capture());
        assertSyncLoadUrlParams(mLoadUrlParamsCaptor.getValue());
    }
}
