// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.ui.base.TestActivity;

/** Tests logic in the {@link AutomotiveUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutomotiveUtilsUnitTest {

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Resources mResources;

    @Mock private Configuration mConfiguration;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testGetHorizontalAutomotiveToolbarHeightDp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.forceHorizontalAutomotiveToolbarForTesting(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Activity spyActivity = spy(activity);
                            doReturn(mResources).when(spyActivity).getResources();
                            doReturn(mConfiguration).when(mResources).getConfiguration();
                            mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
                            int horizontalAutomotiveToolbarHeightDp =
                                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(
                                            spyActivity);
                            assertTrue(
                                    "Horizontal automotive toolbar height should be greater than"
                                            + " 0.",
                                    horizontalAutomotiveToolbarHeightDp > 0);
                        });

        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Activity spyActivity = spy(activity);
                            doReturn(mResources).when(spyActivity).getResources();
                            doReturn(mConfiguration).when(mResources).getConfiguration();
                            mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
                            int horizontalAutomotiveToolbarHeightDp =
                                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(
                                            spyActivity);
                            assertEquals(
                                    "Automotive toolbar should not exist on non automotive"
                                            + " devices.",
                                    0,
                                    horizontalAutomotiveToolbarHeightDp);
                        });
    }

    @Test
    public void testGetVerticalAutomotiveToolbarWidthDp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Activity spyActivity = spy(activity);
                            doReturn(mResources).when(spyActivity).getResources();
                            doReturn(mConfiguration).when(mResources).getConfiguration();
                            mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
                            int verticalAutomotiveToolbarWidthDp =
                                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(
                                            spyActivity);
                            assertTrue(
                                    "Vertical automotive toolbar width should be greater than 0.",
                                    verticalAutomotiveToolbarWidthDp > 0);
                        });

        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Activity spyActivity = spy(activity);
                            doReturn(mResources).when(spyActivity).getResources();
                            doReturn(mConfiguration).when(mResources).getConfiguration();
                            mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
                            int verticalAutomotiveToolbarWidthDp =
                                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(
                                            spyActivity);
                            assertEquals(
                                    "Automotive toolbar should not exist on non automotive"
                                            + " devices.",
                                    0,
                                    verticalAutomotiveToolbarWidthDp);
                        });
    }
}
