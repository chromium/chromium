// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtra;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoUtils.EnhancedProtectionPromoAction;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit test for {@link EnhancedProtectionPromoController}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD})
@Features.DisableFeatures({ChromeFeatureList.QUERY_TILES})
public class EnhancedProtectionPromoTest {
    private static final String METRICS_ENHANCED_PROTECTION_PROMO =
            "NewTabPage.Promo.EnhancedProtectionPromo";
    private static final String METRICS_ENHANCED_PROTECTION_PROMO_IMPRESSION_ACTION =
            "NewTabPage.Promo.EnhancedProtectionPromo.ImpressionUntilAction";
    private static final String METRICS_ENHANCED_PROTECTION_PROMO_IMPRESSION_DISMISSAL =
            "NewTabPage.Promo.EnhancedProtectionPromo.ImpressionUntilDismissal";
    private static final int NTP_HEADER_POSITION = 0;

    private boolean mHasEnhancedProtectionPromoDismissed; // Test value before the test.
    private UserActionTester mActionTester;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();

        // Set promo not dismissed.
        mHasEnhancedProtectionPromoDismissed =
                EnhancedProtectionPromoUtils.isPromoDismissedInSharedPreference();
        EnhancedProtectionPromoUtils.setPromoDismissedInSharedPreference(false);

        SignInPromo.setDisablePromoForTests(true);
    }

    @After
    public void tearDown() {
        EnhancedProtectionPromoUtils.setPromoDismissedInSharedPreference(
                mHasEnhancedProtectionPromoDismissed);
        if (mActionTester != null) mActionTester.tearDown();
        SignInPromo.setDisablePromoForTests(false);
    }

    /**
     * Test that the enhanced protection promo should show for users not signed up for enhanced
     * protection
     */
    @Test
    @SmallTest
    public void testSetUp_Basic() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        View enhancedProtectionPromo =
                mActivityTestRule.getActivity().findViewById(R.id.enhanced_protection_promo);
        Assert.assertNotNull(
                "Enhanced Protection promo should be added to NTP.", enhancedProtectionPromo);
        Assert.assertEquals("Enhanced Protection promo should be visible.", View.VISIBLE,
                enhancedProtectionPromo.getVisibility());

        Assert.assertEquals("Promo created should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_ENHANCED_PROTECTION_PROMO, EnhancedProtectionPromoAction.CREATED));
    }

    @Test
    @MediumTest
    public void testSetUp_NoShow() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.SAFE_BROWSING_ENHANCED, true);
        });
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        View enhancedProtectionPromo =
                mActivityTestRule.getActivity().findViewById(R.id.enhanced_protection_promo);
        Assert.assertNull(
                "Enhanced Protection promo should not be added to NTP.", enhancedProtectionPromo);
    }

    @Test
    @MediumTest
    public void testAcceptImpl() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Intents.init();

        scrollToEnhancedProtectionPromo();

        onView(withId(R.id.promo_primary_button)).perform(click());

        Matcher<Intent> isCorrectComponent = hasComponent(SettingsActivity.class.getName());
        Matcher<Intent> isCorrectFragment =
                hasExtra("show_fragment", SafeBrowsingSettingsFragment.class.getName());
        intended(allOf(isCorrectComponent, isCorrectFragment));

        Assert.assertEquals("Promo accepted should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_ENHANCED_PROTECTION_PROMO, EnhancedProtectionPromoAction.ACCEPTED));
        Assert.assertEquals("Promo impression until action should be recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        METRICS_ENHANCED_PROTECTION_PROMO_IMPRESSION_ACTION));
        Intents.release();
    }

    @Test
    @MediumTest
    public void testDismissImpl() {
        mActionTester = new UserActionTester();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToEnhancedProtectionPromo();

        onView(withId(R.id.promo_secondary_button)).perform(click());
        Assert.assertNull("Enhanced Protection promo should be removed after dismissed.",
                mActivityTestRule.getActivity().findViewById(R.id.enhanced_protection_promo));
        Assert.assertEquals("Promo dismissed should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(METRICS_ENHANCED_PROTECTION_PROMO,
                        EnhancedProtectionPromoAction.DISMISSED));
        Assert.assertEquals("Promo impression until dismissed should be recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        METRICS_ENHANCED_PROTECTION_PROMO_IMPRESSION_DISMISSAL));
        boolean action_tracked = false;
        for (String action : mActionTester.getActions()) {
            if (action.startsWith("NewTabPage.Promo.EnhancedProtectionPromo.Dismissed")) {
                action_tracked = true;
            }
        }
        Assert.assertTrue(action_tracked);

        // Load to NTP one more time. The promo should not show.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        launchNewTabPage();
        Assert.assertNull("Enhanced Protection promo should not be added to NTP.",
                mActivityTestRule.getActivity().findViewById(R.id.enhanced_protection_promo));
    }

    private void scrollToEnhancedProtectionPromo() {
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(NTP_HEADER_POSITION + 1));
        waitForView((ViewGroup) mActivityTestRule.getActivity().findViewById(
                            R.id.enhanced_protection_promo),
                allOf(withId(R.id.promo_primary_button), isDisplayed()));

        CriteriaHelper.pollUiThread(() -> {
            // Verify impression tracking metrics is working.
            Criteria.checkThat("Promo created should be seen.",
                    RecordHistogram.getHistogramValueCountForTesting(
                            METRICS_ENHANCED_PROTECTION_PROMO, EnhancedProtectionPromoAction.SEEN),
                    Matchers.is(1));
            Criteria.checkThat("Impression should be tracked in shared preference.",
                    SharedPreferencesManager.getInstance().readInt(
                            EnhancedProtectionPromoUtils.getTimesSeenKey()),
                    Matchers.is(1));
        });
    }

    private void launchNewTabPage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
    }
}
