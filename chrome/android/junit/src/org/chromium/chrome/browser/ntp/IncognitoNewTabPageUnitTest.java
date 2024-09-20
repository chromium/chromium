// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
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

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.base.TestActivity;

/** Unit test for {@link org.chromium.chrome.browser.ntp.IncognitoNewTabPage} */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
public class IncognitoNewTabPageUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule rule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock NativePageHost mHost;
    @Mock Profile mProfile;
    @Mock DestroyableObservableSupplier<Rect> mMarginSupplier;
    @Mock IncognitoNewTabPageManager mIncognitoNtpManager;

    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mEdgePadAdjusterCaptor;

    private TestActivity mActivity;
    private IncognitoNewTabPage mIncognitoNtp;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setup() {
        mScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        doReturn(true).when(mProfile).isOffTheRecord();

        doReturn(mActivity).when(mHost).getContext();
        doReturn(mMarginSupplier).when(mHost).createDefaultMarginSupplier();

        IncognitoNewTabPage.setIncognitoNtpManagerForTesting(mIncognitoNtpManager);

        mIncognitoNtp = new IncognitoNewTabPage(mActivity, mHost, mProfile, mEdgeToEdgeSupplier);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
    })
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
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
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
    @DisableFeatures(ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE)
    public void setupEdgeToEdgeWithFeatureDisabled() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController, never()).registerAdjuster(any());
    }
}
