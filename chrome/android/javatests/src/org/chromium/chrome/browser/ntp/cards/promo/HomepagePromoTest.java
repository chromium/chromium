// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.nullValue;
import static org.mockito.Mockito.times;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.ntp.cards.promo.HomepagePromoUtils.HomepagePromoAction;
import org.chromium.chrome.browser.partnercustomizations.BasePartnerBrowserCustomizationIntegrationTestRule;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

/**
 * Unit test for {@link HomepagePromoController}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.HOMEPAGE_PROMO_CARD)
@Features.
DisableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.REPORT_FEED_USER_ACTIONS})
public class HomepagePromoTest {
    public static final String PARTNER_HOMEPAGE_URL = "http://127.0.0.1:8000/foo.html";
    public static final String CUSTOM_TEST_URL = "http://127.0.0.1:8000/bar.html";

    private static final String METRICS_HOMEPAGE_PROMO = "NewTabPage.Promo.HomepagePromo";
    private static final String METRICS_HOMEPAGE_PROMO_IMPRESSION_ACTION =
            "NewTabPage.Promo.HomepagePromo.ImpressionUntilAction";
    private static final String METRICS_HOMEPAGE_PROMO_IMPRESSION_DISMISSAL =
            "NewTabPage.Promo.HomepagePromo.ImpressionUntilDismissal";
    private static final int NTP_HEADER_POSITION = 0;

    private boolean mHasHomepagePromoDismissed; // Test value before the test.
    private boolean mHasSignInPromoDismissed; // Test value before the test.

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();
    @Rule
    public BasePartnerBrowserCustomizationIntegrationTestRule mPartnerTestRule =
            new BasePartnerBrowserCustomizationIntegrationTestRule();
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private Tracker mTracker;
    @Mock
    private HomepagePromoVariationManager mMockVariationManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Set promo not dismissed.
        mHasHomepagePromoDismissed = HomepagePromoUtils.isPromoDismissedInSharedPreference();
        HomepagePromoUtils.setPromoDismissedInSharedPreference(false);

        mHasSignInPromoDismissed = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);

        // Set up mock tracker
        Mockito.when(mTracker.shouldTriggerHelpUI(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE))
                .thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        HomepagePromoVariationManager.setInstanceForTesting(mMockVariationManager);

        // By default, use default homepage just like a first time user.
        mHomepageTestRule.useDefaultHomepageForTest();
        SnackbarManager.setDurationForTesting(1000);

        // Start activity first. Doing this to make SignInManager to allow signInPromo to show up.
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        SignInPromo.setDisablePromoForTests(false);
        HomepagePromoUtils.setPromoDismissedInSharedPreference(mHasHomepagePromoDismissed);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, mHasSignInPromoDismissed);
        HomepagePromoVariationManager.setInstanceForTesting(null);
    }

    /**
     * Test that the home page promo should show for users with default homepage.
     * If the user change the homepage, the promo should be gone.
     */
    @Test
    @SmallTest
    public void testSetUp_Basic() {
        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        Assert.assertTrue("Homepage Promo should match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));

        View homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull("Homepage promo should be added to NTP.", homepagePromo);
        Assert.assertEquals(
                "Homepage promo should be visible.", View.VISIBLE, homepagePromo.getVisibility());

        Assert.assertEquals("Promo created should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.CREATED));

        // Change the homepage from homepage manager, so that state listeners get the update.
        // The promo should be gone from the screen.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            HomepageManager.getInstance().setHomepagePreferences(false, false, CUSTOM_TEST_URL);
        });
        homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull("Promo should be gone after homepage changes.", homepagePromo);
        Mockito.verify(mTracker, times(1)).dismissed(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);

        // Different than Tracker system, promo dismissed should not be recorded in histogram.
        Assert.assertEquals("Promo dismissed should not be called.", 0,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.DISMISSED));
    }

    @Test
    @SmallTest
    public void testSetUp_HasSignInPromo() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        Assert.assertTrue("Homepage Promo should match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));
        Assert.assertNull("Homepage promo should not show on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNotNull("SignInPromo should be displayed on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        Assert.assertEquals("Promo created should not be recorded yet.", 0,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.CREATED));

        // Dismiss the signInPromo, and refresh NTP, the homepage promo should show up.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);

        mActivityTestRule.loadUrlInNewTab("about:blank");
        launchNewTabPage();
        Assert.assertNull("SignInPromo should be dismissed.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        View homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull(
                "Homepage promo should be added to NTP when SignInPromo dismissed.", homepagePromo);

        Assert.assertEquals("Promo created should be recorded once.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.CREATED));
    }

    @Test
    @SmallTest
    public void testSetUp_SuppressingSignInPromo() {
        // Adding another check in case Mockito does not prepare the mock correctly.
        Mockito.doReturn(true).when(mMockVariationManager).isSuppressingSignInPromo();
        Assert.assertTrue(mMockVariationManager.isSuppressingSignInPromo());

        launchNewTabPage();

        Assert.assertTrue("Homepage Promo should match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));

        View homepagePromo = mActivityTestRule.getActivity().findViewById(R.id.homepage_promo);
        Assert.assertNotNull("Homepage promo should be added to NTP.", homepagePromo);
        Assert.assertEquals(
                "Homepage promo should be visible.", View.VISIBLE, homepagePromo.getVisibility());

        Assert.assertEquals("Promo created should be recorded once.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.CREATED));

        // Dismiss the HomepagePromo in shared preference, then load another NTP page.
        HomepagePromoUtils.setPromoDismissedInSharedPreference(true);
        mActivityTestRule.loadUrlInNewTab("about:blank");
        launchNewTabPage();

        Assert.assertNotNull("SignInPromo should be displayed on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));
        Assert.assertFalse("Homepage Promo should not match creation criteria.",
                HomepagePromoUtils.shouldCreatePromo(null));
        Assert.assertNull("Homepage promo should not show on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.INTEREST_FEED_V2})
    public void testToggleFeed_WithSignIn() {
        if (FeedFeatures.isV2Enabled()) {
            return; // FeedV1 must not be available.
        }
        // Test to toggle stream when HomepagePromo is hide. Toggle feed should hide promo still.
        launchNewTabPage();

        Assert.assertNull("Homepage promo should not show for this NTP visit.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNotNull("SignInPromo should be displayed on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        NewTabPage ntp = getNewTabPage();
        RecyclerView recyclerView =
                (RecyclerView) ntp.getCoordinatorForTesting().getStreamForTesting().getView();

        toggleFeedHeader(NTP_HEADER_POSITION + 1, recyclerView, false);

        Assert.assertNull("Homepage promo should not show for this NTP visit.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNull("SignInPromo should be hidden from the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        toggleFeedHeader(NTP_HEADER_POSITION + 1, recyclerView, true);

        Assert.assertNull("Homepage promo should not show for this NTP visit.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNotNull("SignInPromo should be displayed on the screen.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1115870")
    public void testToggleFeed_WithHomepage() {
        // Test to toggle NTP when feed is hidden.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.ARTICLES_LIST_VISIBLE, false);
        });
        launchNewTabPage();

        Assert.assertNotNull("Homepage promo should show.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNull("SignInPromo should not displayed on the screen as feed collapse.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        NewTabPage ntp = getNewTabPage();
        RecyclerView recyclerView =
                (RecyclerView) ntp.getCoordinatorForTesting().getStreamForTesting().getView();
        int feedHeaderPosition = NTP_HEADER_POSITION + 2;

        // As HomepagePromo exists, feed header is at
        toggleFeedHeader(feedHeaderPosition, recyclerView, true);

        Assert.assertNotNull("Homepage promo should keep being displayed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNull("SignInPromo should be hidden from the screen for this NTP visit.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));

        toggleFeedHeader(feedHeaderPosition, recyclerView, false);

        Assert.assertNotNull("Homepage promo should keep being displayed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertNull("SignInPromo should be hidden from the screen for this NTP visit.",
                mActivityTestRule.getActivity().findViewById(R.id.signin_promo_view_container));
    }

    /**
     * Clicking on dismiss button should hide the promo.
     */
    @Test
    @MediumTest
    public void testDismiss_Compact() {
        // In order to dismiss the promo, we have to use the large / compact variation.
        setVariationForTests(LayoutStyle.COMPACT);
        testDismissImpl();
    }

    @Test
    @MediumTest
    public void testDismiss_Large() {
        // In order to dismiss the promo, we have to use the large / compact variation.
        setVariationForTests(LayoutStyle.LARGE);
        testDismissImpl();
    }

    private void testDismissImpl() {
        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToHomepagePromo();

        onView(withId(R.id.promo_secondary_button)).perform(click());
        Assert.assertNull("Homepage promo should be removed after dismissed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Assert.assertEquals("Promo dismissed should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.DISMISSED));
        Assert.assertEquals("Promo impression until dismissed should be recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        METRICS_HOMEPAGE_PROMO_IMPRESSION_DISMISSAL));

        Mockito.verify(mTracker, times(1)).dismissed(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);

        // Load to NTP one more time. The promo should not show.
        mActivityTestRule.loadUrlInNewTab("about:blank");
        launchNewTabPage();
        Assert.assertNull("Homepage promo should not be added to NTP.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
    }

    /**
     * Test dismiss by swipe left. This dismissal works in general for any variations.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1084756, crbug.com/1130564")
    public void testDismiss_SwipeToDismiss() {
        setVariationForTests(LayoutStyle.SLIM);

        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToHomepagePromo();

        // Swipe left and wait until the promo is dismissed.
        ViewAction swipeLeft = new GeneralSwipeAction(
                Swipe.FAST, GeneralLocation.CENTER, GeneralLocation.CENTER_LEFT, Press.FINGER);
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        NTP_HEADER_POSITION + 1, swipeLeft));
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().findViewById(R.id.homepage_promo), nullValue());
        });

        // Verification for metrics
        Assert.assertEquals("Promo dismissed should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.DISMISSED));
        Assert.assertEquals("Promo impression until dismissed should be recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        METRICS_HOMEPAGE_PROMO_IMPRESSION_DISMISSAL));

        Mockito.verify(mTracker, times(1)).dismissed(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O, message = "crbug.com/1077316")
    public void testChangeHomepageAndUndo_Compact() {
        setVariationForTests(LayoutStyle.COMPACT);
        testChangeHomepageAndUndoImpl();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O, message = "crbug.com/1077316")
    public void testChangeHomepageAndUndo_Slim() {
        setVariationForTests(LayoutStyle.SLIM);
        testChangeHomepageAndUndoImpl();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O, message = "crbug.com/1077316")
    public void testChangeHomepageAndUndo_Large() {
        setVariationForTests(LayoutStyle.LARGE);
        testChangeHomepageAndUndoImpl();
    }

    private void testChangeHomepageAndUndoImpl() {
        SignInPromo.setDisablePromoForTests(true);
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        scrollToHomepagePromo();

        // Click on the promo card to set the homepage to NTP.
        onView(withId(R.id.promo_primary_button)).perform(click());
        Assert.assertEquals("Promo accepted should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.ACCEPTED));
        Assert.assertEquals("Promo impression until action should be recorded once.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        METRICS_HOMEPAGE_PROMO_IMPRESSION_ACTION));
        Mockito.verify(mTracker, times(1)).notifyEvent(EventConstants.HOMEPAGE_PROMO_ACCEPTED);

        // The promo should be dismissed for feature engagement after homepage changed.
        Assert.assertNull("Homepage promo should be removed.",
                mActivityTestRule.getActivity().findViewById(R.id.homepage_promo));
        Mockito.verify(mTracker, times(1)).dismissed(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);

        Assert.assertTrue("Homepage should be set to NTP after clicking on promo primary button.",
                UrlUtilities.isNTPUrl(HomepageManager.getHomepageUri()));

        // Verifications on snackbar, and click on "undo".
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        Snackbar snackbar = snackbarManager.getCurrentSnackbarForTesting();
        Assert.assertEquals("Undo snackbar not shown.", Snackbar.UMA_HOMEPAGE_PROMO_CHANGED_UNDO,
                snackbar.getIdentifierForTesting());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { snackbar.getController().onAction(snackbar.getActionDataForTesting()); });
        Assert.assertEquals("Promo undo should be recorded once. ", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        METRICS_HOMEPAGE_PROMO, HomepagePromoAction.UNDO));
        Assert.assertEquals("Homepage should change back to partner homepage after undo.",
                HomepageManager.getHomepageUri(), PARTNER_HOMEPAGE_URL);
    }

    @Test
    @SmallTest
    public void testExperimentTrackerSignals() {
        // Tagging for synthetic trial should happen once.
        Mockito.verify(mMockVariationManager, times(1)).tagSyntheticHomepagePromoSeenGroup();

        mHomepageTestRule.useChromeNTPForTest();
        ToolbarManager toolbarManager = mActivityTestRule.getActivity().getToolbarManager();

        if (toolbarManager != null) {
            HomeButton homeButton = toolbarManager.getHomeButtonForTesting();
            onViewWaiting(allOf(is(homeButton), isDisplayed()));
            if (homeButton != null) {
                ChromeTabUtils.waitForTabPageLoaded(
                        mActivityTestRule.getActivity().getActivityTab(), UrlConstants.NTP_URL,
                        () -> { TouchCommon.singleClickView(homeButton); });

                Mockito.verify(mTracker).notifyEvent(EventConstants.NTP_SHOWN);
                Mockito.verify(mTracker).notifyEvent(EventConstants.NTP_HOME_BUTTON_CLICKED);
            }
        }
    }

    private void scrollToHomepagePromo() {
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(NTP_HEADER_POSITION + 1));
        waitForView((ViewGroup) mActivityTestRule.getActivity().findViewById(R.id.homepage_promo),
                allOf(withId(R.id.promo_primary_button), isDisplayed()));

        CriteriaHelper.pollUiThread(() -> {
            // Verify impress tracking metrics is working.
            Criteria.checkThat("Promo created should be seen.",
                    RecordHistogram.getHistogramValueCountForTesting(
                            METRICS_HOMEPAGE_PROMO, HomepagePromoAction.SEEN),
                    Matchers.is(1));
            Criteria.checkThat("Impression should be tracked in shared preference.",
                    SharedPreferencesManager.getInstance().readInt(
                            HomepagePromoUtils.getTimesSeenKey()),
                    Matchers.is(1));
        });
        Mockito.verify(mTracker).notifyEvent(EventConstants.HOMEPAGE_PROMO_SEEN);
    }

    private void setVariationForTests(@LayoutStyle int variation) {
        Mockito.when(mMockVariationManager.getLayoutVariation()).thenReturn(variation);
    }

    private void launchNewTabPage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);
    }

    private NewTabPage getNewTabPage() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
        return (NewTabPage) tab.getNativePage();
    }

    private void toggleFeedHeader(int feedHeaderPosition, ViewGroup rootView, boolean expanded) {
        if (FeedFeatures.isV2Enabled()) {
            onView(allOf(instanceOf(RecyclerView.class), withId(R.id.feed_stream_recycler_view)))
                    .perform(RecyclerViewActions.scrollToPosition(feedHeaderPosition));
            onView(withId(R.id.header_menu)).perform(click());

            onView(withText(expanded ? R.string.ntp_turn_on_feed : R.string.ntp_turn_off_feed))
                    .perform(click());

            onView(withText(expanded ? R.string.ntp_discover_on : R.string.ntp_discover_off))
                    .check(ViewAssertions.matches(isDisplayed()));
        } else {
            onView(instanceOf(RecyclerView.class))
                    .perform(RecyclerViewActions.scrollToPosition(feedHeaderPosition),
                            RecyclerViewActions.actionOnItemAtPosition(
                                    feedHeaderPosition, click()));

            // Scroll to the same position in case the refresh brings the section header out of
            // the screen.
            onView(instanceOf(RecyclerView.class))
                    .perform(RecyclerViewActions.scrollToPosition(feedHeaderPosition));
            waitForView(rootView,
                    allOf(withId(R.id.header_status),
                            withText(expanded ? R.string.hide_content : R.string.show_content)));
        }
    }
}
