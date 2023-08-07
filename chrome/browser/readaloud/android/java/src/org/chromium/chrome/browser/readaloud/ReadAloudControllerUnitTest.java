// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudControllerUnitTest {
    private static final GURL sTestGURL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    private MockTab mTab;
    private ReadAloudController mController;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock
    private ObservableSupplier<Profile> mMockProfileSupplier;
    @Mock
    private Profile mMockProfile;
    @Mock
    Context mContext;
    @Mock
    private ReadAloudReadabilityHooksImpl mHooksImpl;

    MockTabModelSelector mTabModelSelector;

    @Captor
    ArgumentCaptor<ReadAloudReadabilityHooks.ReadabilityCallback> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockProfile).when(mMockProfileSupplier).get();

        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);

        mTabModelSelector = new MockTabModelSelector(
                /* tabCount= */ 2, /* incognitoTabCount= */ 1, (id, incognito) -> {
                    Tab tab = spy(MockTab.createAndInitialize(id, incognito));
                    return tab;
                });
        when(mHooksImpl.isEnabled()).thenReturn(true);
        ReadAloudController.setReadabilityHooks(mHooksImpl);
        mController = new ReadAloudController(
                mContext, mMockProfileSupplier, mTabModelSelector.getModel(false));

        mTab = (MockTab) mTabModelSelector.getCurrentTab();
        mTab.setGurlOverrideForTesting(sTestGURL);
    }

    @Test
    public void testIsAvailable() {
        // test set up: non incognito profile + MSBB Accepted
        assertTrue(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_offTheRecord() {
        when(mMockProfile.isOffTheRecord()).thenReturn(true);
        assertFalse(mController.isAvailable());
    }

    @Test
    public void testIsAvailable_noMSBB() {
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        assertFalse(mController.isAvailable());
    }

    // Helper function for checkReadabilityOnPageLoad_URLnotReadAloudSupported() to check
    // the provided url is recognized as unreadable
    private void checkURLNotReadAloudSupported(GURL url) {
        mTab.setGurlOverrideForTesting(url);

        mController.getTabModelTabObserver().onPageLoadStarted(mTab, mTab.getUrl());

        verify(mHooksImpl, never())
                .isPageReadable(Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadabilityOnPageLoad_URLnotReadAloudSupported() {
        checkURLNotReadAloudSupported(new GURL("invalid"));
        checkURLNotReadAloudSupported(GURL.emptyGURL());
        checkURLNotReadAloudSupported(new GURL("chrome://history/"));
        checkURLNotReadAloudSupported(new GURL("about:blank"));
        checkURLNotReadAloudSupported(new GURL("https://www.google.com/search?q=weather"));
        checkURLNotReadAloudSupported(new GURL("https://myaccount.google.com/"));
        checkURLNotReadAloudSupported(new GURL("https://myactivity.google.com/"));
    }

    @Test
    public void checkReadabilityOnPageLoad_success() {
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onSuccess(sTestGURL.getSpec(), true, false);
        assertTrue(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we don't resend a request
        mTab = (MockTab) mTabModelSelector.getModel(false).getTabAt(1);
        mTab.setGurlOverrideForTesting(sTestGURL);
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void checkReadabilityOnPageLoad_onlyOnePendingRequest() {
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
    }

    @Test
    public void checkReadabilityOnPageLoad_failure() {
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());
        assertFalse(mController.isReadable(mTab));

        mCallbackCaptor.getValue().onFailure(
                sTestGURL.getSpec(), new Throwable("Something went wrong"));
        assertFalse(mController.isReadable(mTab));
        assertFalse(mController.timepointsSupported(mTab));

        // now check that the second time the same url loads we will resend a request
        mTab = (MockTab) mTabModelSelector.getModel(false).getTabAt(1);
        mTab.setGurlOverrideForTesting(sTestGURL);
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(2))
                .isPageReadable(Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }

    @Test
    public void testReactingtoMSBBChange() {
        mController.getTabModelTabObserver().onPageLoadStarted(mTab, sTestGURL);

        verify(mHooksImpl, times(1))
                .isPageReadable(eq(sTestGURL.getSpec()), mCallbackCaptor.capture());

        // Disable MSBB. Sending requests to Google servers no longer allowed but using
        // previous results is ok.
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(false);
        mController.getTabModelTabObserver().onPageLoadStarted(
                mTab, JUnitTestGURLs.getGURL(JUnitTestGURLs.GOOGLE_URL_CAT));

        verify(mHooksImpl, times(1))
                .isPageReadable(Mockito.anyString(),
                        Mockito.any(ReadAloudReadabilityHooks.ReadabilityCallback.class));
    }
}
