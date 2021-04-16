// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.app.NotificationManager;
import android.content.res.Configuration;
import android.os.Build;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/** End-to-end tests for PriceAlertsMessageCard. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        "force-fieldtrials=Study/Group"})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.DisableFeatures({ChromeFeatureList.START_SURFACE_ANDROID})
public class PriceAlertsMessageCardTest {
    // clang-format on
    private static final String BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_price_notification/true";
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public IntentsTestRule<ChromeActivity> mIntentTestRule =
            new IntentsTestRule<>(ChromeActivity.class, false, false);

    @Before
    public void setUp() {
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);
        mMockNotificationManager = new MockNotificationManagerProxy();
        PriceDropNotificationManager.setNotificationManagerForTesting(mMockNotificationManager);
        mPriceDropNotificationManager = new PriceDropNotificationManager();

        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.deleteChannelForTesting();
        }
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(null);
        PriceDropNotificationManager.setNotificationManagerForTesting(null);
        ActivityUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testMessageCardShowing() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(PriceTrackingUtilities.isPriceDropNotificationEligible());
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.canPostNotification());
        assertTrue(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testMessageCardNotShowing_AlreadyCanPostNotification() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(PriceTrackingUtilities.isPriceDropNotificationEligible());
        mMockNotificationManager.setNotificationsEnabled(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.createNotificationChannel();
        }
        assertTrue(mPriceDropNotificationManager.canPostNotification());
        // We don't show this message card if user can already receive price drop notifications.
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());

        enterTabSwitcher(cta);
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testMessageCardNotShowing_MessageDisabled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(PriceTrackingUtilities.isPriceDropNotificationEligible());
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.canPostNotification());
        PriceTrackingUtilities.disablePriceAlertsMessageCard();
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());

        enterTabSwitcher(cta);
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:enable_price_notification/false"})
    public void testMessageCardNotShowing_ParameterDisabled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertFalse(PriceTrackingUtilities.isPriceDropNotificationEligible());
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.canPostNotification());
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());

        enterTabSwitcher(cta);
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testReviewMessage_AppNotificationsEnabled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(true);
        assertNull(mPriceDropNotificationManager.getNotificationChannel());

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        onView(allOf(withId(R.id.action_button),
                       withParent(withId(R.id.large_message_linear_layout))))
                .perform(click());
        assertNotNull(mPriceDropNotificationManager.getNotificationChannel());
        assertEquals(NotificationManager.IMPORTANCE_LOW,
                mPriceDropNotificationManager.getNotificationChannel().getImportance());
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testReviewMessage_AppNotificationsDisabled() {
        Intents.init();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        onView(allOf(withId(R.id.action_button),
                       withParent(withId(R.id.large_message_linear_layout))))
                .perform(click());
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            intended(hasAction(ACTION_APP_NOTIFICATION_SETTINGS));
        } else {
            intended(hasAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS));
        }
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        Intents.release();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testDismissMessage() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.large_message_card_view))))
                .perform(click());
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testSwipeMessage() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        RecyclerView.ViewHolder viewHolder = ((RecyclerView) cta.findViewById(R.id.tab_list_view))
                                                     .findViewHolderForAdapterPosition(1);
        assertEquals(TabProperties.UiType.LARGE_MESSAGE, viewHolder.getItemViewType());

        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.compositor_view_holder))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));
        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRemoveMessageWhenClosingLastTab() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        closeFirstTabInTabSwitcher();
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());

        // Exit & re-enter the tab switcher, the message should show.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testDisableMessageAfterShowingTenTimes() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        for (int i = 0; i < 10; i++) {
            assertTrue(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
            enterTabSwitcher(cta);
            CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
            onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
            clickFirstCardFromTabSwitcher(cta);
        }

        assertFalse(PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        enterTabSwitcher(cta);
        onView(withId(R.id.large_message_card_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testRenderMessageCard_Portrait_AppNotificationsEnabled() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(true);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.large_message_card_item),
                "price_alerts_message_portrait_app_notifications_enabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderMessageCard_Portrait_AppNotificationsDisabled() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.large_message_card_item),
                "price_alerts_message_portrait_app_notifications_disabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testRenderMessageCard_Landscape_AppNotificationsEnabled() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(true);

        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.large_message_card_item),
                "price_alerts_message_landscape_app_notifications_enabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderMessageCard_Landscape_AppNotificationsDisabled() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mMockNotificationManager.setNotificationsEnabled(false);

        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(cta.findViewById(R.id.large_message_card_item),
                "price_alerts_message_landscape_app_notifications_disabled");
    }
}