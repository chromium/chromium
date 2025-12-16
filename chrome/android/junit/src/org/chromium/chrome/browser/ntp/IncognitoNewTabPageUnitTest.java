// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.widget.ScrollView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link org.chromium.chrome.browser.ntp.IncognitoNewTabPage} */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoNewTabPageUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock NativePageHost mHost;
    @Mock Profile mProfile;
    @Mock Tab mTab;
    @Mock Destroyable mMarginSupplier;
    @Mock IncognitoNewTabPageManager mIncognitoNtpManager;
    @Mock IncognitoNtpMetrics mIncognitoNtpMetrics;

    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mEdgePadAdjusterCaptor;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private TestActivity mActivity;
    private IncognitoNewTabPage mIncognitoNtp;
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setup() {
        mScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        doReturn(mProfile).when(mTab).getProfile();
        doReturn(true).when(mProfile).isOffTheRecord();

        doReturn(mActivity).when(mHost).getContext();
        doReturn(mMarginSupplier).when(mHost).createDefaultMarginAdapter(any());

        IncognitoNewTabPage.setIncognitoNtpManagerForTesting(mIncognitoNtpManager);

        mIncognitoNtp =
                new IncognitoNewTabPage(
                        mActivity, mHost, mTab, mEdgeToEdgeSupplier, mIncognitoNtpMetrics);
    }

    @Test
    public void setupEdgeToEdgeWithInsets() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mEdgePadAdjusterCaptor.capture());

        // Simulate a new bottom insets is set.
        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(100);

        ScrollView view = mIncognitoNtp.mIncognitoNewTabPageView.getScrollView();
        assertEquals("Bottom padding should be set. ", 100, view.getPaddingBottom());
        assertFalse(
                "ScrollView should not clip to padding under E2E mode.", view.getClipToPadding());
    }

    @Test
    public void setupEdgeToEdgeWithoutInsets() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mEdgePadAdjusterCaptor.capture());
        assertTrue("Incognito NTP should support E2E.", mIncognitoNtp.supportsEdgeToEdge());

        // Simulate a new bottom insets is set.
        mEdgePadAdjusterCaptor.getValue().overrideBottomInset(0);

        ScrollView view = mIncognitoNtp.mIncognitoNewTabPageView.getScrollView();
        assertEquals("Bottom padding should be set. ", 0, view.getPaddingBottom());
        assertTrue(
                "ScrollView should be clip to padding where there's no bottom insets.",
                view.getClipToPadding());
    }

    @Test
    public void recordTimeToFirstNavigation() {
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();
        assertNotNull(observer);

        observer.onPageLoadStarted(mTab, JUnitTestGURLs.EXAMPLE_URL);

        verify(mIncognitoNtpMetrics).recordNavigatedAway();
        verify(mTab).removeObserver(observer);
    }
}
