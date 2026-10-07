// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.assertNoSearchResultsFound;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.clickSearchResult;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.highlighted;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.typeSearchQuery;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.ui.test.util.RenderTestRule;

/** Tests for {@link PersonalizeGoogleServicesSettings}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Affects sign-in state, which is global.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PersonalizeGoogleServicesSettingsTest {
    private static final int RENDER_TEST_REVISION = 1;

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsTestRule<PersonalizeGoogleServicesSettings> mSettingsTestRule =
            new SettingsTestRule<>(PersonalizeGoogleServicesSettings.class);

    private final SettingsTestRule<MainSettings> mSettingsSearchTestRule =
            new SettingsTestRule<>(null);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule)
                    .around(mSettingsTestRule)
                    .around(mSettingsSearchTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Rule public MockitoRule rule = MockitoJUnit.rule();

    @Mock private GoogleActivityController mGoogleActivityController;
    @Mock private RegionalCapabilitiesService mRegionalCapabilities;

    @Before
    public void setUp() {
        ServiceLoaderUtil.setInstanceForTesting(
                GoogleActivityController.class, mGoogleActivityController);
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);
        when(mRegionalCapabilities.isInEeaCountry()).thenReturn(true);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PersonalizedGoogleServices"})
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testLayout() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsTestRule.startSettingsActivity();
        PersonalizeGoogleServicesSettings fragment = mSettingsTestRule.getFragment();
        ChromeRenderTestRule.sanitize(fragment.getView());
        mRenderTestRule.render(fragment.getView(), "personalize_google_services");
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickWebAndAppActivity() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsTestRule.startSettingsActivity();

        onView(withText(R.string.personalized_google_services_waa_title)).perform(click());
        verify(mGoogleActivityController).openWebAndAppActivitySettings(any(), any());
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testClickLinkedGoogleServices() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsTestRule.startSettingsActivity();

        onView(withText(R.string.personalized_google_services_linked_services_title))
                .perform(click());
        verify(mGoogleActivityController).openLinkedGoogleServicesSettings(any(), any());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky test: b/334206890")
    @Feature({"PersonalizedGoogleServices"})
    public void testUserNotSignedIn() {
        mSettingsTestRule.startSettingsActivity();
        // The activity should terminate immediately if the user is not signed in.
        ApplicationTestUtils.waitForActivityState(mSettingsTestRule.getActivity(), Stage.DESTROYED);
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testSearchWebAndAppActivity_signedIn() {
        mSettingsSearchTestRule.startSettingsActivity();
        mSyncTestRule.setUpAccountAndSignInForTesting();

        typeSearchQuery("app activity");

        clickSearchResult(R.string.personalized_google_services_waa_title);

        onView(highlighted(R.string.personalized_google_services_waa_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testSearchLinkedGoogleServices_signedIn() {
        mSettingsSearchTestRule.startSettingsActivity();
        mSyncTestRule.setUpAccountAndSignInForTesting();

        typeSearchQuery("linked google services");

        clickSearchResult(R.string.personalized_google_services_linked_services_title);

        onView(highlighted(R.string.personalized_google_services_linked_services_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testSearchPersonalizeGoogleServices_nonEea() {
        when(mRegionalCapabilities.isInEeaCountry()).thenReturn(false);
        mSettingsSearchTestRule.startSettingsActivity();
        mSyncTestRule.setUpAccountAndSignInForTesting();

        typeSearchQuery("linked google services");

        assertNoSearchResultsFound();
    }

    @Test
    @SmallTest
    @Feature({"PersonalizedGoogleServices"})
    public void testSearchPersonalizeGoogleServices_signedOut() {
        mSettingsSearchTestRule.startSettingsActivity();

        typeSearchQuery("linked google services");

        assertNoSearchResultsFound();
    }
}
