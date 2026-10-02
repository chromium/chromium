// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.automotive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.util.AutomotiveUtils;
import org.chromium.ui.base.TestActivity;

/** Tests logic in the {@link AutomotiveUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutomotiveUtilsUnitTest {

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Test
    public void testGetHorizontalAutomotiveToolbarHeightDp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AutomotiveUtils.forceHorizontalAutomotiveToolbarForTesting(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int horizontalAutomotiveToolbarHeightDp =
                                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(
                                            activity);
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
                            int horizontalAutomotiveToolbarHeightDp =
                                    AutomotiveUtils.getHorizontalAutomotiveToolbarHeightDp(
                                            activity);
                            assertEquals(
                                    "Automotive toolbar should not exist on non automotive"
                                            + " devices.",
                                    0,
                                    horizontalAutomotiveToolbarHeightDp);
                        });
    }

    @Test
    @Config(qualifiers = "land")
    public void testGetVerticalAutomotiveToolbarWidthDp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int verticalAutomotiveToolbarWidthDp =
                                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(activity);
                            assertTrue(
                                    "Vertical automotive toolbar width should be greater than 0.",
                                    verticalAutomotiveToolbarWidthDp > 0);
                        });

        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int verticalAutomotiveToolbarWidthDp =
                                    AutomotiveUtils.getVerticalAutomotiveToolbarWidthDp(activity);
                            assertEquals(
                                    "Automotive toolbar should not exist on non automotive"
                                            + " devices.",
                                    0,
                                    verticalAutomotiveToolbarWidthDp);
                        });
    }
}
