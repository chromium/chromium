// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.desktop_popup_header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.app.Activity;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link DesktopPopupHeaderLayoutCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DesktopPopupHeaderLayoutCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private ObservableSupplier<Tab> mTabSupplier;

    private Activity mActivity;
    private FrameLayout mParentView;
    private ViewStub mViewStub;
    private DesktopPopupHeaderLayoutCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // ViewStub must have a parent to inflate correctly.
                    mParentView = new FrameLayout(mActivity);
                    mActivity.setContentView(mParentView);

                    mViewStub = new ViewStub(mActivity);
                    mParentView.addView(mViewStub);

                    mCoordinator =
                            new DesktopPopupHeaderLayoutCoordinator(
                                    mViewStub,
                                    mDesktopWindowStateManager,
                                    mTabSupplier,
                                    /* isIncognito= */ false,
                                    mActivity);
                });
    }

    @Test
    @SmallTest
    public void testCreation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
                });
    }

    @Test
    @SmallTest
    public void testDestroy() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Ensure destroy() runs without throwing exceptions.
                    mCoordinator.destroy();
                });
    }
}
