// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link CrossDeviceListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDeviceListCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private CrossDeviceListCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity -> {
                            mActivity = activity;
                            mCoordinator = new CrossDeviceListCoordinator(mActivity);
                        }));
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
    }

    @Test
    @SmallTest
    public void testGetView() {
        assertNotNull(mCoordinator.getView());
    }
}
