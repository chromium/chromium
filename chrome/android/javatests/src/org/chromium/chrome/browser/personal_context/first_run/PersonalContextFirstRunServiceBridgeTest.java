// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.personal_context.first_run;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** End-to-end integration tests for {@link PersonalContextFirstRunService} using real JNI. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests use different command-line flags to force enablement state.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PersonalContextFirstRunServiceBridgeTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Profile mProfile;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = Profile.fromWebContents(mActivityTestRule.getWebContents());
                });
    }

    /**
     * Test 1: Verify shouldShowAmbientAutofillNotice is true under state 2 (eligible), and calling
     * ambientAutofillNoticeAcknowledged makes it false.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=PersonalContextFirstRunNoticePhase2,"
                + "PersonalContextForceEnablementState:state/2"
    })
    public void testAmbientAutofillNoticeEligibleInitially() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Under state 2 (eligible), shouldShowAmbientAutofillNotice must return true
                    // initially.
                    Assert.assertTrue(
                            PersonalContextFirstRunService.shouldShowAmbientAutofillNotice(
                                    mProfile));

                    // Call real C++ ambientAutofillNoticeAcknowledged JNI.
                    PersonalContextFirstRunService.ambientAutofillNoticeAcknowledged(mProfile);

                    // Notice should no longer be shown.
                    Assert.assertFalse(
                            PersonalContextFirstRunService.shouldShowAmbientAutofillNotice(
                                    mProfile));
                });
    }

    /**
     * Test 2: Verify shouldShowAtMemoryNotice is true under state 2 (eligible), and calling
     * atMemoryNoticeAcknowledged makes it false.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=PersonalContextFirstRunNoticePhase2,"
                + "PersonalContextForceEnablementState:state/2"
    })
    public void testAtMemoryNoticeEligibleInitially() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Under state 2 (eligible), shouldShowAtMemoryNotice must return true
                    // initially.
                    Assert.assertTrue(
                            PersonalContextFirstRunService.shouldShowAtMemoryNotice(mProfile));

                    // Call real C++ atMemoryNoticeAcknowledged JNI.
                    PersonalContextFirstRunService.atMemoryNoticeAcknowledged(mProfile);

                    // Notice should no longer be shown.
                    Assert.assertFalse(
                            PersonalContextFirstRunService.shouldShowAtMemoryNotice(mProfile));
                });
    }
}
