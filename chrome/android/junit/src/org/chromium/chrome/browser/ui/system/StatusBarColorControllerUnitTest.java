// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.view.Window;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.base.TestActivity;

/* Unit tests for StatusBarColorController behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusBarColorControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private EdgeToEdgeSystemBarColorHelper mSystemBarColorHelper;
    private Window mWindow;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mWindow = activity.getWindow();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
    public void testSetStatusBarColor_EdgeToEdgeEnabled() {
        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper).setStatusBarColor(Color.BLUE);

        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.RED);
        verify(mSystemBarColorHelper).setStatusBarColor(Color.RED);
    }

    @Test
    public void testSetStatusBarColor_EdgeToEdgeDisabled() {
        StatusBarColorController.setStatusBarColor(mSystemBarColorHelper, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper, times(0)).setStatusBarColor(anyInt());

        StatusBarColorController.setStatusBarColor(null, mWindow, Color.BLUE);
        verify(mSystemBarColorHelper, times(0)).setStatusBarColor(anyInt());
    }
}
