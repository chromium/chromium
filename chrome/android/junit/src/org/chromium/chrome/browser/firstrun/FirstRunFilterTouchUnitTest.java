// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.signin.SigninFeatures;

import java.util.Arrays;
import java.util.Collection;

/**
 * TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNT_PART2 launch.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
})
@EnableFeatures({
    ChromeFeatureList.CCT_REPORT_PRERENDER_EVENTS,
    ChromeFeatureList.XPLAT_SYNCED_SETUP
})
public class FirstRunFilterTouchUnitTest {

    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Parameters(name = "{index}_isIdentityMgrMigration={0}")
    public static Collection parameters() {
        return Arrays.asList(true, false);
    }

    @Rule
    public final ActivityScenarioRule<FirstRunActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(FirstRunActivity.class);

    private FirstRunActivity mActivity;

    public FirstRunFilterTouchUnitTest(boolean isIdentityManagerMigrationEnabled) {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                isIdentityManagerMigrationEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
    }

    @Test
    @SmallTest
    public void testShouldPreventTouch() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        assertFalse("Events should be accepted.", mActivity.shouldPreventTouch());
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        assertTrue("Events should be discarded.", mActivity.shouldPreventTouch());
    }
}
