// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

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

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Tests {@link CustomTabActivity} filters touch events from overlay activity. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
@EnableFeatures(ChromeFeatureList.CCT_REPORT_PRERENDER_EVENTS)
public class CustomTabsFilterTouchUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    private CustomTabActivity mActivity;

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
