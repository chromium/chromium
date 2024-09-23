// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.PersonalizeGoogleServicesSettings;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.RenderTestRule;

/** Tests for {@link PersonalizeGoogleServicesSettings}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PersonalizeGoogleServicesSettingsTest {
    private static final int RENDER_TEST_REVISION = 0;

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<PersonalizeGoogleServicesSettings>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(PersonalizeGoogleServicesSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock private GoogleActivityController mGoogleActivityController;

    @Before
    public void setUp() {
        ServiceLoaderUtil.setInstanceForTesting(
                GoogleActivityController.class, mGoogleActivityController);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PersonalizedGoogleServices"})
    public void testLayout() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();
        PersonalizeGoogleServicesSettings fragment = mSettingsActivityTestRule.getFragment();
        ChromeRenderTestRule.sanitize(fragment.getView());
        mRenderTestRule.render(fragment.getView(), "personalize_google_services");
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickWebAndAppActivity() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.personalized_google_services_waa_title)).perform(click());
        verify(mGoogleActivityController).openWebAndAppActivitySettings(any(), any());
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickLinkedGoogleServices() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.personalized_google_services_linked_services_title))
                .perform(click());
        verify(mGoogleActivityController).openLinkedGoogleServicesSettings(any(), any());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky test: b/334206890")
    @Feature({"PersonalizedGoogleServices"})
    public void testUserNotSignedIn() {
        mSettingsActivityTestRule.startSettingsActivity();
        // The activity should terminate immediately if the user is not signed in.
        ApplicationTestUtils.waitForActivityState(
                mSettingsActivityTestRule.getActivity(), Stage.DESTROYED);
    }
}
