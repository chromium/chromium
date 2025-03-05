// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.data_sharing.DataSharingServiceImpl;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;

/** Instrumentation tests for {@link CollaborationService}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.COLLABORATION_FLOW_ANDROID})
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/399444939) Re-enable on automotive devices if needed.
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class CollaborationIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private Profile mProfile;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
                    FirstRunStatus.setFirstRunFlowComplete(true);
                });
    }

    @Test
    @MediumTest
    public void testDataSharing() {
        GURL url =
                DataSharingServiceImpl.getDataSharingUrlForTesting(
                        new GroupToken("collaboration_id", "access_token"));
        mActivityTestRule.loadUrlInNewTab(url.getSpec());
        onView(withText(R.string.collaboration_signin_description)).check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(ViewActions.click());
    }
}
