// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Tests for {@link MerchantTrustDetailsTabMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustDetailsTabMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private BottomSheetController mMockBottomSheetController;

    @Mock
    private GURL mMockDestinationGurl;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;

    @Mock
    private NavigationController mMockNavigationController;

    @Mock
    private ContentView mMockContentView;
    @Mock
    private MerchantTrustDetailsSheetContent mMockSheetContent;

    @Mock
    private Profile mMockProfile;

    @Mock
    private MerchantTrustMetrics mMockMetrics;

    @Captor
    private ArgumentCaptor<WebContentsDelegateAndroid> mWebContentsDelegateCaptor;

    private static final String DUMMY_SHEET_TITLE = "DUMMY_TITLE";
    private static final String DUMMY_URL = "dummy://visible/url";

    @Mock
    SecurityStateModel.Natives mSecurityStateMocks;
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(DUMMY_URL).when(mMockDestinationGurl).getSpec();
        doReturn(mMockDestinationGurl).when(mMockWebContents).getVisibleUrl();

        doReturn(mMockNavigationController).when(mMockWebContents).getNavigationController();
        when(mUrlUtilitiesJniMock.isGoogleDomainUrl(anyString(), anyBoolean())).thenReturn(true);

        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mocker.mock(SecurityStateModelJni.TEST_HOOKS, mSecurityStateMocks);
    }

    @Test
    public void testRequestShowContent() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);

        verify(mMockSheetContent, times(1)).setTitle(eq(DUMMY_SHEET_TITLE));

        verify(mMockBottomSheetController, times(1))
                .requestShowContent(eq(mMockSheetContent), eq(true));
    }

    @Test(expected = java.lang.AssertionError.class)
    public void testRequestShowContentNonGoogleUrl() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        doReturn(false).when(mUrlUtilitiesJniMock).isGoogleDomainUrl(anyString(), anyBoolean());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);
    }

    @Test(expected = java.lang.AssertionError.class)
    public void testRequestShowContentBeforeInitIsCalled() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        doReturn(true).when(mUrlUtilitiesJniMock).isGoogleDomainUrl(anyString(), anyBoolean());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);
    }

    @Test
    public void testInit() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);

        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        any(WebContentsDelegateAndroid.class));
    }

    @Test
    public void testSslChanges() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);

        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        mWebContentsDelegateCaptor.capture());

        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);
        mWebContentsDelegateCaptor.getValue().visibleSSLStateChanged();

        verify(mMockSheetContent, times(1)).setUrl(eq(mMockDestinationGurl));
        verify(mMockSheetContent, times(1)).setSecurityIcon(any(Integer.class));
    }

    @Test
    public void testWebContentsOpenNewTab() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        mWebContentsDelegateCaptor.capture());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);
        mWebContentsDelegateCaptor.getValue().openNewTab(mMockDestinationGurl, "", null, 0, true);

        verify(mMockNavigationController, times(2)).loadUrl(any(LoadUrlParams.class));
    }

    @Test
    public void testWebContentsDelegateShouldCreateWebContents() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        mWebContentsDelegateCaptor.capture());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);

        assertFalse(mWebContentsDelegateCaptor.getValue().shouldCreateWebContents(
                mMockDestinationGurl));

        verify(mMockNavigationController, times(2)).loadUrl(any(LoadUrlParams.class));
    }

    @Test
    public void testGetTopControlsHeight() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        mWebContentsDelegateCaptor.capture());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);

        assertEquals(100, mWebContentsDelegateCaptor.getValue().getTopControlsHeight());
    }

    @Test
    public void testLoadingStateChanges() {
        MerchantTrustDetailsTabMediator instance = getMediatorUnderTest();
        instance.init(mMockWebContents, mMockContentView, mMockSheetContent, mMockProfile);
        verify(mMockSheetContent, times(1))
                .attachWebContents(eq(mMockWebContents), eq(mMockContentView),
                        mWebContentsDelegateCaptor.capture());
        instance.requestShowContent(mMockDestinationGurl, DUMMY_SHEET_TITLE);

        // Loading state.
        doReturn(true).when(mMockWebContents).isLoading();
        mWebContentsDelegateCaptor.getValue().loadingStateChanged(true);

        verify(mMockSheetContent, times(1)).setProgress(eq(0f));
        verify(mMockSheetContent, times(1)).setProgressVisible(eq(true));
    }

    private MerchantTrustDetailsTabMediator getMediatorUnderTest() {
        return new MerchantTrustDetailsTabMediator(mMockBottomSheetController, 100, mMockMetrics);
    }
}