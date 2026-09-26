// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.app.Activity;
import android.view.ViewStub;
import android.widget.FrameLayout;

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
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link DesktopPopupHeaderLayoutCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DesktopPopupHeaderLayoutCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;

    private Activity mActivity;
    private FrameLayout mParentView;
    private ViewStub mViewStub;
    private DesktopPopupHeaderLayoutCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();

        // ViewStub must have a parent to inflate correctly.
        mParentView = new FrameLayout(mActivity);
        mActivity.setContentView(mParentView);

        mViewStub = new ViewStub(mActivity);
        mParentView.addView(mViewStub);

        mCoordinator =
                new DesktopPopupHeaderLayoutCoordinator(
                        mViewStub,
                        mDesktopWindowStateManager,
                        /* tabSupplier= */ ObservableSuppliers.alwaysNull(),
                        /* isIncognito= */ false,
                        mActivity);
    }

    @Test
    public void testCreation() {

        // Since ViewStub is final, we cannot mock it to verify
        // setLayoutResource/inflate calls. Instead, we verify the outcome: the ViewStub
        // should have been replaced by the inflated view in its parent.
        assertEquals(
                "Parent should still have exactly one child (the inflated view)",
                1,
                mParentView.getChildCount());
        assertNotEquals(
                "ViewStub should have been replaced by the inflated view",
                mViewStub,
                mParentView.getChildAt(0));
    }

    @Test
    public void testDestroy() {

        // Ensure destroy() runs without throwing exceptions.
        mCoordinator.destroy();
    }
}
