// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link HubCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubCoordinatorUnitTest {
    public @Rule ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private ObservableSupplierImpl<Pane> mPaneSupplier = new ObservableSupplierImpl<>();
    private FrameLayout mRootView;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mRootView = new FrameLayout(activity);
        activity.setContentView(mRootView);
    }

    @Test
    @SmallTest
    public void testCreateAndDestroy() {
        HubCoordinator hubCoordinator = new HubCoordinator(mRootView, mPaneSupplier);
        mRootView.getChildCount();
        Assert.assertNotEquals(0, mRootView.getChildCount());
        hubCoordinator.destroy();
        Assert.assertEquals(0, mRootView.getChildCount());
    }
}
