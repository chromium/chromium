// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.widget.LinearLayout;
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
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Unit test for {@link org.chromium.chrome.browser.ntp.IncognitoNewTabPage} */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoNewTabPageUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock NativePageHost mHost;
    @Mock Profile mProfile;
    @Mock Destroyable mMarginSupplier;
    @Mock IncognitoNewTabPageManager mIncognitoNtpManager;

    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mEdgePadAdjusterCaptor;

    private TestActivity mActivity;
    private IncognitoNewTabPage mIncognitoNtp;
    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setup() {
        mScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        doReturn(true).when(mProfile).isOffTheRecord();

        doReturn(mActivity).when(mHost).getContext();
        doReturn(mMarginSupplier).when(mHost).createDefaultMarginAdapter(any());

        IncognitoNewTabPage.setIncognitoNtpManagerForTesting(mIncognitoNtpManager);

        mIncognitoNtp =
                new IncognitoNewTabPage(
                        mActivity, mHost, mProfile, mEdgeToEdgeSupplier);
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
    public void testDescriptionViewLayoutAdaptsToViewWidth() {
        IncognitoDescriptionView descriptionView =
                mIncognitoNtp.getView().findViewById(R.id.new_tab_incognito_container);
        LinearLayout bulletpointsContainer =
                descriptionView.findViewById(R.id.new_tab_incognito_bulletpoints_container);

        float density = mActivity.getResources().getDisplayMetrics().density;

        // Simulate measuring the view at a narrow width (e.g. 580dp, as when vertical tabs is
        // shown).
        int narrowWidthPx = Math.round(580 * density);
        int heightPx = Math.round(800 * density);

        descriptionView.measure(
                android.view.View.MeasureSpec.makeMeasureSpec(
                        narrowWidthPx, android.view.View.MeasureSpec.EXACTLY),
                android.view.View.MeasureSpec.makeMeasureSpec(
                        heightPx, android.view.View.MeasureSpec.EXACTLY));

        // When view width is <= 720dp, bullet points should be stacked vertically (narrow layout).
        assertEquals(
                "Bullet points should be vertical when view width is narrow.",
                LinearLayout.VERTICAL,
                bulletpointsContainer.getOrientation());

        // Simulate measuring the view at a wide width (e.g. 800dp).
        int wideWidthPx = Math.round(800 * density);
        descriptionView.measure(
                android.view.View.MeasureSpec.makeMeasureSpec(
                        wideWidthPx, android.view.View.MeasureSpec.EXACTLY),
                android.view.View.MeasureSpec.makeMeasureSpec(
                        heightPx, android.view.View.MeasureSpec.EXACTLY));

        // When view width is > 720dp, bullet points should be horizontal (wide layout).
        assertEquals(
                "Bullet points should be horizontal when view width is wide.",
                LinearLayout.HORIZONTAL,
                bulletpointsContainer.getOrientation());
    }
}
