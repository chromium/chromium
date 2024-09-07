// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.recent_tabs.CrossDeviceListProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.CrossDeviceListProperties.EMPTY_STATE_VISIBLE;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.recent_tabs.ui.CrossDevicePaneView;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/** Render tests for the Cross Device Pane. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class CrossDevicePaneRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_RECENT_TABS)
                    .setRevision(1)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final boolean mNightModeEnabled;
    private CrossDeviceListCoordinator mCoordinator;
    private CrossDevicePaneView mView;
    private PropertyModel mModel;
    private FrameLayout mContentView;

    public CrossDevicePaneRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mNightModeEnabled = nightModeEnabled;
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.launchActivity(null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        runOnUiThreadBlocking(
                () -> {
                    mView =
                            (CrossDevicePaneView)
                                    LayoutInflater.from(activity)
                                            .inflate(R.layout.cross_device_pane, null);
                    mModel = new PropertyModel.Builder(ALL_KEYS).build();

                    mContentView = new FrameLayout(activity);
                    mContentView.setBackgroundColor(mNightModeEnabled ? Color.BLACK : Color.WHITE);
                    mContentView.addView(mView);
                    LayoutParams params =
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                    activity.setContentView(mContentView, params);
                });
    }

    @After
    public void tearDownTest() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCrossDevicePane_emptyStateVisible() throws IOException, InterruptedException {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(EMPTY_STATE_VISIBLE, true);
                });

        mRenderTestRule.render(mContentView, "cross_device_pane_empty_state");
    }
}
