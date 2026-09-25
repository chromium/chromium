// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.ViewportInsets;

/** Unit tests for {@link BottomContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomContainerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    private Activity mActivity;
    private BottomContainer mBottomContainer;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mBottomContainer = new BottomContainer(mActivity, null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void testSetTranslationY_EmptyChildDeferredUntilAddView() {
        doReturn(100).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(50).when(mBrowserControlsStateProvider).getBottomControlOffset();

        mBottomContainer.initialize(
                mBrowserControlsStateProvider,
                ObservableSuppliers.createNonNull(new ViewportInsets()),
                ObservableSuppliers.alwaysNull());

        // Initially no children, translationY should remain 0 (not computed/applied).
        assertEquals(0f, mBottomContainer.getTranslationY(), 0.01f);

        // Change controls offset while empty.
        doReturn(20).when(mBrowserControlsStateProvider).getBottomControlOffset();
        mBottomContainer.onControlsOffsetChanged(
                0,
                0,
                /* topControlsMinHeightChanged= */ false,
                20,
                0,
                /* bottomControlsMinHeightChanged= */ false,
                /* requestNewFrame= */ false,
                /* isVisibilityForced= */ false);
        assertEquals(0f, mBottomContainer.getTranslationY(), 0.01f);

        // Add a child view. Translation should now be updated according to controls offset.
        // bottomControlOffset (20) - bottomControlsHeight (100) = -80.
        View child = new View(mActivity);
        mBottomContainer.addView(child);
        assertEquals(-80f, mBottomContainer.getTranslationY(), 0.01f);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void testSetTranslationY_CalculatedWhenEmptyWithoutFlag() {
        doReturn(100).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(50).when(mBrowserControlsStateProvider).getBottomControlOffset();

        mBottomContainer.initialize(
                mBrowserControlsStateProvider,
                ObservableSuppliers.createNonNull(new ViewportInsets()),
                ObservableSuppliers.alwaysNull());

        // Without flag, translation is computed even when empty: 50 - 100 = -50.
        assertEquals(-50f, mBottomContainer.getTranslationY(), 0.01f);
    }
}
