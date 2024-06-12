// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Tests for {@link MerchantTrustSignalsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class MerchantTrustSignalsMediatorTest {

    @Mock private MerchantTrustSignalsMediator.MerchantTrustSignalsCallback mMockDelegate;

    @Mock private Tab mMockTab;

    @Mock private ObservableSupplier<Tab> mMockTabProvider;

    @Mock private WebContents mMockWebContents;

    @Mock private NavigationHandle mMockNavigationHandle;

    @Mock private GURL mMockUrl;

    @Mock private MerchantTrustMetrics mMockMetrics;

    @Captor private ArgumentCaptor<Callback<Tab>> mTabSupplierCallbackCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private MerchantTrustSignalsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doReturn(false).when(mMockTab).isIncognito();
        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInPrimaryMainFrame();
        doReturn(false).when(mMockNavigationHandle).isPrimaryMainFrameFragmentNavigation();
        doReturn(false).when(mMockNavigationHandle).isErrorPage();
        doReturn(mMockUrl).when(mMockNavigationHandle).getUrl();
        doReturn("fake_host").when(mMockUrl).getHost();

        createMediatorAndVerify();
    }

    @After
    public void tearDown() {
        destroyAndVerify();
    }

    private void createMediatorAndVerify() {
        mMediator = new MerchantTrustSignalsMediator(mMockTabProvider, mMockDelegate, mMockMetrics);
        verify(mMockTabProvider, times(1)).addObserver(mTabSupplierCallbackCaptor.capture());
        mTabSupplierCallbackCaptor.getValue().onResult(mMockTab);
        verify(mMockTab, times(1)).addObserver(mTabObserverCaptor.capture());
    }

    private void destroyAndVerify() {
        mMediator.destroy();
        verify(mMockTab, times(1)).removeObserver(mTabObserverCaptor.getValue());
        verify(mMockTabProvider, times(1)).removeObserver(mTabSupplierCallbackCaptor.getValue());
    }

    @Test
    public void testTabObserverOnDidFinishNavigation() {
        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockMetrics, times(1)).updateRecordingMessageImpact(eq("fake_host"));
        verify(mMockDelegate, times(1))
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_IncognitoTab() {
        doReturn(true).when(mMockTab).isIncognito();

        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never())
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_NavigationNonCommit() {
        doReturn(false).when(mMockNavigationHandle).hasCommitted();

        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never())
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_PrimaryMainFrameFragmentNavigation() {
        doReturn(true).when(mMockNavigationHandle).isPrimaryMainFrameFragmentNavigation();

        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never())
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_ErrorPage() {
        doReturn(true).when(mMockNavigationHandle).isErrorPage();

        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never())
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_MissingHost() {
        doReturn(null).when(mMockUrl).getHost();

        mTabObserverCaptor
                .getValue()
                .onDidFinishNavigationInPrimaryMainFrame(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never())
                .onFinishEligibleNavigation(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnHidden() {
        mTabObserverCaptor.getValue().onHidden(mMockTab, TabHidingType.ACTIVITY_HIDDEN);
        verify(mMockMetrics, times(1)).finishRecordingMessageImpact();
    }

    @Test
    public void testTabObserverOnDestroyed() {
        mTabObserverCaptor.getValue().onDestroyed(mMockTab);
        verify(mMockMetrics, times(1)).finishRecordingMessageImpact();
    }
}
