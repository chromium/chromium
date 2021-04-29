// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

/**
 * Tests for {@link MerchantTrustSignalsMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustSignalsMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private MerchantTrustSignalsMediator.MerchantTrustSignalsCallback mMockDelegate;

    @Mock
    private TabModelSelector mMockTabModelSelector;

    @Mock
    private TabModelObserver mMockTabModelObserver;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private TabImpl mMockPrimaryTab;

    @Mock
    private TabImpl mMockSecondaryTab;

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private NavigationHandle mMockNavigationHandle;

    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Captor
    private ArgumentCaptor<WebContentsObserver> mWebContentsObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFilterProvider).when(mMockTabModelSelector).getTabModelFilterProvider();
        doReturn(100).when(mMockPrimaryTab).getId();
        doReturn("fake://url/2").when(mMockPrimaryTab).getUrlString();
        doReturn(mMockWebContents).when(mMockPrimaryTab).getWebContents();

        doReturn(200).when(mMockSecondaryTab).getId();
        doReturn("fake://url/2").when(mMockSecondaryTab).getUrlString();
        doReturn(mMockWebContents).when(mMockSecondaryTab).getWebContents();
    }

    @Test
    public void testObserversSetup() {
        getMediatorUnderTest(mMockPrimaryTab);
    }

    @Test
    public void testWebContentsUpdated() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);

        // Select another tab.
        mTabModelObserverCaptor.getValue().didSelectTab(
                mMockSecondaryTab, TabSelectionType.FROM_USER, 100);

        verify(mMockWebContents, times(1)).removeObserver(eq(mWebContentsObserver.getValue()));
        verify(mMockWebContents, times(2)).addObserver(mWebContentsObserver.capture());
    }

    @Test
    public void testDestroyCleanup() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);

        mediator.destroy();
        verify(mTabModelFilterProvider, times(1))
                .removeTabModelFilterObserver(eq(mTabModelObserverCaptor.getValue()));
    }

    @Test
    public void testWebContentsNavigation() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);
        GURL gurl = mock(GURL.class);

        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInMainFrame();
        doReturn(false).when(mMockNavigationHandle).isSameDocument();

        doReturn("fake_host").when(gurl).getHost();
        doReturn(gurl).when(mMockNavigationHandle).getUrl();

        mWebContentsObserver.getValue().didFinishNavigation(mMockNavigationHandle);

        verify(mMockDelegate, times(1)).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testWebContentsNavigationNonCommit() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);
        GURL gurl = mock(GURL.class);

        doReturn(false).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInMainFrame();
        doReturn(false).when(mMockNavigationHandle).isSameDocument();

        doReturn("fake_host").when(gurl).getHost();
        doReturn(gurl).when(mMockNavigationHandle).getUrl();

        mWebContentsObserver.getValue().didFinishNavigation(mMockNavigationHandle);

        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testWebContentsNavigationNonMainFrame() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);
        GURL gurl = mock(GURL.class);

        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(false).when(mMockNavigationHandle).isInMainFrame();
        doReturn(false).when(mMockNavigationHandle).isSameDocument();

        doReturn("fake_host").when(gurl).getHost();
        doReturn(gurl).when(mMockNavigationHandle).getUrl();

        mWebContentsObserver.getValue().didFinishNavigation(mMockNavigationHandle);

        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testWebContentsNavigationSameDoc() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);
        GURL gurl = mock(GURL.class);

        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInMainFrame();
        doReturn(true).when(mMockNavigationHandle).isSameDocument();

        doReturn("fake_host").when(gurl).getHost();
        doReturn(gurl).when(mMockNavigationHandle).getUrl();

        mWebContentsObserver.getValue().didFinishNavigation(mMockNavigationHandle);

        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testWebContentsNavigationMissingHost() {
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(mMockPrimaryTab);
        GURL gurl = mock(GURL.class);

        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInMainFrame();
        doReturn(false).when(mMockNavigationHandle).isSameDocument();

        doReturn(null).when(gurl).getHost();
        doReturn(gurl).when(mMockNavigationHandle).getUrl();

        mWebContentsObserver.getValue().didFinishNavigation(mMockNavigationHandle);

        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testInitialTabSelection() {
        doReturn(mMockPrimaryTab).when(mMockTabModelSelector).getCurrentTab();
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(null);
        verify(mMockWebContents, times(1)).addObserver(mWebContentsObserver.capture());
    }

    @Test
    public void testInitialTabEmpty() {
        doReturn(null).when(mMockTabModelSelector).getCurrentTab();
        MerchantTrustSignalsMediator mediator = getMediatorUnderTest(null);
        verify(mMockWebContents, never()).addObserver(mWebContentsObserver.capture());
    }

    private MerchantTrustSignalsMediator getMediatorUnderTest(TabImpl tabToSelect) {
        MerchantTrustSignalsMediator mediator =
                new MerchantTrustSignalsMediator(mMockTabModelSelector, mMockDelegate);
        verify(mTabModelFilterProvider, times(1))
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());

        if (tabToSelect != null) {
            mTabModelObserverCaptor.getValue().didSelectTab(
                    mMockPrimaryTab, TabSelectionType.FROM_USER, 0);
            verify(mMockWebContents, times(1)).addObserver(mWebContentsObserver.capture());
        }

        return mediator;
    }
}