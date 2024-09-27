// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DASHBOARD_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerTestHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DashboardInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NotificationsModuleInteractions;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PermissionsModuleInteractions;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Tests for various Safety Hub settings surfaces. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
@Batch(Batch.PER_CLASS)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public final class SafetyHubTest {
    private static final PermissionsData PERMISSIONS_DATA_1 =
            PermissionsData.create(
                    "http://example1.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingsType.MEDIASTREAM_MIC
                    },
                    0,
                    0);

    private static final PermissionsData PERMISSIONS_DATA_2 =
            PermissionsData.create(
                    "http://example2.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        ContentSettingsType.MEDIASTREAM_MIC,
                        ContentSettingsType.GEOLOCATION,
                        ContentSettingsType.BACKGROUND_SYNC
                    },
                    0,
                    0);

    private static final PermissionsData PERMISSIONS_DATA_3 =
            PermissionsData.create(
                    "http://example3.com",
                    new int[] {ContentSettingsType.NOTIFICATIONS, ContentSettingsType.GEOLOCATION},
                    0,
                    0);
    private static final NotificationPermissions NOTIFICATION_PERMISSIONS_1 =
            NotificationPermissions.create("http://example1.com", "*", 3);
    private static final NotificationPermissions NOTIFICATION_PERMISSIONS_2 =
            NotificationPermissions.create("http://example2.com", "*", 8);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public SettingsActivityTestRule<SafetyHubPermissionsFragment> mPermissionsFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubPermissionsFragment.class);

    @Rule
    public SettingsActivityTestRule<SafetyHubNotificationsFragment> mNotificationsFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubNotificationsFragment.class);

    @Rule
    public SettingsActivityTestRule<SafetyHubFragment> mSafetyHubFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubFragment.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .setRevision(1)
                    .build();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private FakeUnusedSitePermissionsBridge mUnusedPermissionsBridge =
            new FakeUnusedSitePermissionsBridge();

    private FakeNotificationPermissionReviewBridge mNotificationPermissionReviewBridge =
            new FakeNotificationPermissionReviewBridge();

    private Profile mProfile;

    private void executeWhileCapturingIntents(Runnable func) {
        Intents.init();
        try {
            Intent intent = new Intent();
            Instrumentation.ActivityResult result =
                    new Instrumentation.ActivityResult(Activity.RESULT_OK, intent);
            intending(anyIntent()).respondWith(result);

            if (func != null) {
                func.run();
            }
        } finally {
            Intents.release();
        }
    }

    private static String getPackageName() {
        return ContextUtils.getApplicationContext().getPackageName();
    }

    @Before
    public void setUp() {
        mJniMocker.mock(UnusedSitePermissionsBridgeJni.TEST_HOOKS, mUnusedPermissionsBridge);
        mJniMocker.mock(
                NotificationPermissionReviewBridgeJni.TEST_HOOKS,
                mNotificationPermissionReviewBridge);

        mActivityTestRule.startMainActivityOnBlankPage();
        mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest", "SafetyHubPermissions"})
    public void testPermissionsSubpageAppearance() throws IOException {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2, PERMISSIONS_DATA_3});
        mPermissionsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getRootViewSanitized(R.string.safety_hub_permissions_page_title),
                "permissions_subpage");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest", "SafetyHubNotifications"})
    public void testNotificationsSubpageAppearance() throws IOException {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {
                    NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2
                });
        mNotificationsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getRootViewSanitized(R.string.safety_hub_notifications_page_title),
                "notifications_subpage");
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionRegrant() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1});
        mPermissionsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .build();

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_1.getOrigin());
        onView(withText(PERMISSIONS_DATA_1.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_1.getOrigin())).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testClearPermissionsReviewList() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.OPEN_REVIEW_UI,
                                PermissionsModuleInteractions.ACKNOWLEDGE_ALL,
                                PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL)
                        .build();

        // Verify the permissions module is displaying the info state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 2, 2);
        scrollToExpandedPreference(permissionsTitle);
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(permissionsTitle, true);

        // Open the permissions subpage.
        clickOnSecondaryButtonNextToText(permissionsTitle);

        // Verify that 2 sites are displayed.
        onView(withText(PERMISSIONS_DATA_1.getOrigin())).check(matches(isDisplayed()));
        onView(withText(PERMISSIONS_DATA_2.getOrigin())).check(matches(isDisplayed()));

        // Click the button at the bottom of the page.
        onView(withText(R.string.got_it)).perform(click());

        // Verify tha the permissions subpage has been dismissed and the state of the permissions
        // module has changed.
        onViewWaiting(withText(R.string.safety_hub_permissions_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the info state is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(permissionsTitle)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionsBottomButtonState() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1});
        mSafetyHubFragmentTestRule.startSettingsActivity();

        // Verify the permissions module is displaying the info state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 1, 1);
        scrollToPreference(withText(permissionsTitle));
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be collapsed initially since it's in an info state.
        verifyButtonsNextToTextVisibility(permissionsTitle, false);
        expandPreferenceWithText(permissionsTitle);

        // Open the permissions subpage.
        scrollToExpandedPreference(permissionsTitle);
        clickOnSecondaryButtonNextToText(permissionsTitle);

        // Check that the button is enabled.
        onView(withText(R.string.got_it)).check(matches(isEnabled()));

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_1.getOrigin());

        // Check that the button is disabled.
        onView(withText(R.string.got_it)).check(matches(not(isEnabled())));

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());

        // Check that the button is enabled.
        onView(withText(R.string.got_it)).check(matches(isEnabled()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionsToSiteSettings() {
        SettingsActivity activity = mPermissionsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                        PermissionsModuleInteractions.GO_TO_SETTINGS);

        openActionBarOverflowOrOptionsMenu(activity);
        onViewWaiting(withText(R.string.safety_hub_go_to_site_settings_button)).perform(click());
        onViewWaiting(withText(R.string.prefs_site_settings)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationAllow() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mNotificationsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                                NotificationsModuleInteractions.IGNORE,
                                NotificationsModuleInteractions.UNDO_IGNORE)
                        .build();

        // Always allow the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_allow_notifications_menu_item)).perform(click());
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationReset() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mNotificationsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                                NotificationsModuleInteractions.RESET,
                                NotificationsModuleInteractions.UNDO_RESET)
                        .build();

        // Reset the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_reset_notifications_menu_item)).perform(click());
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testResetAllNotifications() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {
                    NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2
                });
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                                NotificationsModuleInteractions.OPEN_UI_REVIEW,
                                NotificationsModuleInteractions.BLOCK_ALL,
                                NotificationsModuleInteractions.UNDO_BLOCK_ALL)
                        .build();

        // Verify the notifications module is displaying the info state.
        String notificationsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 2, 2);
        scrollToExpandedPreference(notificationsTitle);
        onView(withText(notificationsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(notificationsTitle, true);

        // Open the notifications subpage.
        clickOnSecondaryButtonNextToText(notificationsTitle);

        // Verify that 2 sites are displayed.
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));
        onView(withText(NOTIFICATION_PERMISSIONS_2.getPrimaryPattern()))
                .check(matches(isDisplayed()));

        // Click the button at the bottom of the page.
        onView(withText(R.string.safety_hub_notifications_reset_all_button)).perform(click());

        // Verify tha the notifications subpage has been dismissed and the state of the
        // notification module has changed.
        onViewWaiting(withText(R.string.safety_hub_notifications_review_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the info state is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(notificationsTitle)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationsBottomButtonState() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mSafetyHubFragmentTestRule.startSettingsActivity();

        // Verify the notifications module is displaying the info state.
        String notificationsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 1, 1);
        scrollToPreference(withText(notificationsTitle));
        scrollToExpandedPreference(notificationsTitle);
        onView(withText(notificationsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(notificationsTitle, true);

        // Open the notifications subpage.
        clickOnSecondaryButtonNextToText(notificationsTitle);

        // Check that the button is enabled.
        onView(withText(R.string.safety_hub_notifications_reset_all_button))
                .check(matches(isEnabled()));

        // Reset the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_reset_notifications_menu_item)).perform(click());

        // Check that the button is disabled.
        onView(withText(R.string.safety_hub_notifications_reset_all_button))
                .check(matches(not(isEnabled())));

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());

        // Check that the button is enabled.
        onView(withText(R.string.safety_hub_notifications_reset_all_button))
                .check(matches(isEnabled()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationsToNotificationSettings() {
        SettingsActivity activity = mNotificationsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                        NotificationsModuleInteractions.GO_TO_SETTINGS);

        openActionBarOverflowOrOptionsMenu(activity);
        onViewWaiting(withText(R.string.safety_hub_go_to_notification_settings_button))
                .perform(click());
        onViewWaiting(
                        allOf(
                                withText(R.string.push_notifications_permission_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubSafeBrowsing"})
    public void testSafeBrowsingModule() {
        setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                        DashboardInteractions.GO_TO_SAFE_BROWSING_SETTINGS);

        // Verify the safe browsing module is displaying the enhanced protection state.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.safety_hub_safe_browsing_enhanced_title);
        scrollToPreference(withText(safeBrowsingTitle));
        onView(withText(safeBrowsingTitle)).check(matches(isDisplayed()));

        // Module should be collapsed initially since it's in a safe state.
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, false);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Expand the module to show the buttons.
        expandPreferenceWithText(safeBrowsingTitle);

        // Click on the secondary button and verity that the Safe Browsing settings is opened.
        scrollToExpandedPreference(safeBrowsingTitle);
        clickOnSecondaryButtonNextToText(safeBrowsingTitle);
        onViewWaiting(withText(R.string.prefs_safe_browsing_title)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubUpdateCheck"})
    @CommandLineFlags.Add({
        ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE + "=update_available",
    })
    public void testUpdateCheckModule() {
        // TODO(crbug.com/324562205): Move the initialization of the SafetyHubFetchService so
        // that there is no dependency on ChromeTabbedActivity.
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify the update check module is displaying the "Update available" state.
        String updateCheckTitle =
                safetyHubFragment.getString(R.string.safety_check_updates_outdated);
        scrollToExpandedPreference(updateCheckTitle);
        onView(withText(updateCheckTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in a warning state.
        verifyButtonsNextToTextVisibility(updateCheckTitle, true);

        if (BuildConfig.IS_CHROME_BRANDED) {
            var histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                            DashboardInteractions.OPEN_PLAY_STORE);
            executeWhileCapturingIntents(
                    () -> {
                        // Open the Play Store.
                        clickOnPrimaryButtonNextToText(updateCheckTitle);

                        intended(
                                IntentMatchers.hasData(
                                        Uri.parse(
                                                ContentUrlConstants.PLAY_STORE_URL_PREFIX
                                                        + getPackageName())));
                    });
            histogramWatcher.assertExpected();
        }
    }

    @Test
    @MediumTest
    public void testPreferenceExpand() {
        setSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify the safe browsing module is displaying the no protection state.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        onView(withText(safeBrowsingTitle)).check(matches(isDisplayed()));

        // The module should be expanded in it's initial state.
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, true);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Click on collapse button.
        expandPreferenceWithText(safeBrowsingTitle);
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, false);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Click on expand button.
        expandPreferenceWithText(safeBrowsingTitle);
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, true);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @RequiresRestart
    public void testMultiplePreferenceExpand() {
        // Set a module with an unmanaged warning state.
        int compromisedPasswordsCount = 5;
        addCredentialToAccountStore();
        setCompromisedPasswordsCount(compromisedPasswordsCount);

        // Set a module with info state.
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {
                    NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2
                });

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify only the unmanaged warning state module should be expanded by default.
        String passwordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        scrollToExpandedPreference(passwordsTitle);
        verifyButtonsNextToTextVisibility(passwordsTitle, true);

        // Verify other modules are collapsed.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, false);

        String notificationsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 2, 2);
        scrollToPreference(withText(notificationsTitle));
        verifyButtonsNextToTextVisibility(notificationsTitle, false);

        // Fix the warning state
        setCompromisedPasswordsCount(0);
        mSafetyHubFragmentTestRule.recreateActivity();
        safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        passwordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_compromised_passwords_title);
        scrollToPreference(withText(passwordsTitle));
        verifyButtonsNextToTextVisibility(passwordsTitle, false);

        // Verify info modules are now expanded.
        scrollToExpandedPreference(safeBrowsingTitle);
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, true);

        scrollToExpandedPreference(notificationsTitle);
        verifyButtonsNextToTextVisibility(notificationsTitle, true);
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionsModule_ClearList() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ACKNOWLEDGE_ALL,
                                PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL)
                        .build();

        // Verify the permissions module is displaying the info state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 2, 2);
        scrollToExpandedPreference(permissionsTitle);
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(permissionsTitle, true);

        // Click on the Got it button and verify the permissions module has changed to a safe
        // state.
        clickOnPrimaryButtonNextToText(permissionsTitle);
        onViewWaiting(withText(R.string.safety_hub_permissions_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the info state is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(permissionsTitle)).check(matches(isDisplayed()));

        List<PermissionsData> permissionsList =
                Arrays.asList(
                        mUnusedPermissionsBridge.getRevokedPermissions(
                                safetyHubFragment.getProfile()));
        assertEquals(2, permissionsList.size());
        assertThat(permissionsList, containsInAnyOrder(PERMISSIONS_DATA_1, PERMISSIONS_DATA_2));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionsModule_SafeState() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(new PermissionsData[] {});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                        PermissionsModuleInteractions.GO_TO_SETTINGS);

        // Verify the permissions module is displaying the safe state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_permissions_ok_title);

        scrollToPreference(withText(permissionsTitle));
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be collapsed initially since it's in a safe state.
        verifyButtonsNextToTextVisibility(permissionsTitle, false);
        expandPreferenceWithText(permissionsTitle);

        // Click on the secondary button and verify that the site settings page is opened.
        scrollToExpandedPreference(permissionsTitle);
        clickOnSecondaryButtonNextToText(permissionsTitle);
        onViewWaiting(withText(R.string.prefs_site_settings)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationReviewModule_ResetAll() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {
                    NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2
                });
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                                NotificationsModuleInteractions.BLOCK_ALL,
                                NotificationsModuleInteractions.UNDO_BLOCK_ALL)
                        .build();

        // Verify the notifications module is displaying the info state.
        String notificationsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 2, 2);

        scrollToExpandedPreference(notificationsTitle);
        onView(withText(notificationsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(notificationsTitle, true);

        // Click on the reset all button and verify the notification module has changed to a
        // safe state.
        clickOnPrimaryButtonNextToText(notificationsTitle);
        onViewWaiting(withText(R.string.safety_hub_notifications_review_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the notifications are allowed
        // again and the info state is displayed.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(notificationsTitle)).check(matches(isDisplayed()));

        List<NotificationPermissions> notificationPermissions =
                Arrays.asList(
                        mNotificationPermissionReviewBridge.getNotificationPermissions(
                                safetyHubFragment.getProfile()));
        assertEquals(2, notificationPermissions.size());
        assertThat(
                notificationPermissions,
                containsInAnyOrder(NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationReviewModule_SafeState() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME,
                        NotificationsModuleInteractions.GO_TO_SETTINGS);

        // Verify the notifications module is displaying the safe state.
        String notificationsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_notifications_review_ok_title);

        scrollToPreference(withText(notificationsTitle));
        onView(withText(notificationsTitle)).check(matches(isDisplayed()));

        // Module should be collapsed initially since it's in a safe state.
        verifyButtonsNextToTextVisibility(notificationsTitle, false);
        expandPreferenceWithText(notificationsTitle);

        // Click on the secondary button and verify that notifications site settings page is
        // opened.
        scrollToExpandedPreference(notificationsTitle);
        clickOnSecondaryButtonNextToText(notificationsTitle);
        onViewWaiting(
                        allOf(
                                withText(R.string.push_notifications_permission_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubTips"})
    public void testSafetyTipsPreferenceExpand() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify the safety tips module is displayed.
        String safetyTipsTitle =
                safetyHubFragment.getString(R.string.safety_hub_safety_tips_section_header);
        scrollToPreference(withText(safetyTipsTitle));
        onView(withText(safetyTipsTitle)).check(matches(isDisplayed()));

        // The module should be collapsed in it's initial state.
        // Verify the child preferences are not visible.
        onView(withText(R.string.safety_hub_safety_tips_safety_tools_title)).check(doesNotExist());
        onView(withText(R.string.safety_hub_safety_tips_incognito_title)).check(doesNotExist());
        onView(withText(R.string.safety_hub_safety_tips_safe_browsing_title)).check(doesNotExist());

        // Click on expand button.
        expandPreferenceWithText(safetyTipsTitle);
        scrollToLastPosition();

        // Verify the child preferences are now visible.
        onView(withText(R.string.safety_hub_safety_tips_safety_tools_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.safety_hub_safety_tips_incognito_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.safety_hub_safety_tips_safe_browsing_title))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubTips"})
    public void testSafetyToolsLearnMoreLink_OpensInCCT() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                        DashboardInteractions.OPEN_SAFETY_TOOLS_INFO);
        scrollToLastPosition();

        String safetyTipsTitle =
                safetyHubFragment.getString(R.string.safety_hub_safety_tips_section_header);
        scrollToPreference(withText(safetyTipsTitle));

        // The module should be collapsed in it's initial state and the children are hidden.
        // Click on expand button.
        expandPreferenceWithText(safetyTipsTitle);
        scrollToLastPosition();

        // The module should be expanded in it's initial state and all its children are visible.
        // Verify the Safety tools preference is displayed and clicking on it opens the correct
        // link in CCT.
        String safetyToolsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_safety_tips_safety_tools_title);
        onView(withText(safetyToolsTitle)).check(matches(isDisplayed()));
        clickOnPreferenceWithTextAndWaitForActivity(
                withText(safetyToolsTitle), SafetyHubFragment.SAFETY_TOOLS_LEARN_MORE_URL);
        pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubTips"})
    public void testIncognitoLearnMoreLink_OpensInCCT() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                        DashboardInteractions.OPEN_INCOGNITO_INFO);
        scrollToLastPosition();

        String safetyTipsTitle =
                safetyHubFragment.getString(R.string.safety_hub_safety_tips_section_header);
        scrollToPreference(withText(safetyTipsTitle));

        // The module should be collapsed in it's initial state and the children are hidden.
        // Click on expand button.
        expandPreferenceWithText(safetyTipsTitle);
        scrollToLastPosition();

        // The module should be expanded in it's initial state and all its children are visible.
        // Verify the Incognito preference is displayed and clicking on it opens the correct
        // link in CCT.
        String incognitoTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_safety_tips_incognito_title);
        onViewWaiting(withText(incognitoTitle)).check(matches(isDisplayed()));
        clickOnPreferenceWithTextAndWaitForActivity(
                withText(incognitoTitle), SafetyHubFragment.INCOGNITO_LEARN_MORE_URL);
        pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubTips"})
    public void testSafeBrowsingLearnMoreLink_OpensInCCT() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                        DashboardInteractions.OPEN_SAFE_BROWSING_INFO);
        scrollToLastPosition();

        String safetyTipsTitle =
                safetyHubFragment.getString(R.string.safety_hub_safety_tips_section_header);
        scrollToPreference(withText(safetyTipsTitle));

        // The module should be collapsed in it's initial state and the children are hidden.
        // Click on expand button.
        expandPreferenceWithText(safetyTipsTitle);
        scrollToLastPosition();

        // Verify the Safe browsing preference is displayed and clicking on it opens the correct
        // link in CCT.
        String safeBrowsingTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_safety_tips_safe_browsing_title);
        onView(withText(safeBrowsingTitle)).check(matches(isDisplayed()));
        clickOnPreferenceWithTextAndWaitForActivity(
                withText(safeBrowsingTitle), SafetyHubFragment.SAFE_BROWSING_LEARN_MORE_URL);
        pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testHelpCenterArticle() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DASHBOARD_INTERACTIONS_HISTOGRAM_NAME,
                        DashboardInteractions.OPEN_HELP_CENTER);

        // Verify the help center info button is displayed and clicking on it opens the correct
        // link in CCT.
        onView(withId(R.id.menu_id_targeted_help)).check(matches(isDisplayed()));
        clickOnPreferenceWithTextAndWaitForActivity(
                withId(R.id.menu_id_targeted_help), SafetyHubFragment.HELP_CENTER_URL);
        pressBack();

        histogramWatcher.assertExpected();
    }

    private void clickOnPreferenceWithTextAndWaitForActivity(
            Matcher<View> matcher, String expectedUrl) {
        CustomTabActivity cta =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        CustomTabActivity.class,
                        () -> onView(matcher).perform(click()));

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = cta.getActivityTab();
                    Criteria.checkThat(tab, Matchers.notNullValue());
                    Criteria.checkThat(tab.getUrl().getSpec(), is(expectedUrl));
                });
    }

    private void clickOnButtonNextToText(String text) {
        onViewWaiting(allOf(withId(R.id.button), withParent(hasSibling(withChild(withText(text))))))
                .perform(click());
    }

    private void clickOnPrimaryButtonNextToText(String text) {
        onViewWaiting(
                        allOf(
                                withId(R.id.primary_button),
                                withParent(hasSibling(withChild(withChild(withText(text)))))))
                .perform(click());
    }

    private void clickOnSecondaryButtonNextToText(String text) {
        onViewWaiting(
                        allOf(
                                withId(R.id.secondary_button),
                                withParent(hasSibling(withChild(withChild(withText(text)))))))
                .perform(click());
    }

    private void expandPreferenceWithText(String text) {
        onView(withText(text)).perform(click());
    }

    private void verifyButtonsNextToTextVisibility(String text, boolean visible) {
        onView(
                        allOf(
                                withId(R.id.buttons_container),
                                hasSibling(withChild(withChild(withText(text))))))
                .check(matches(visible ? isDisplayed() : not(isDisplayed())));
    }

    private void verifySummaryNextToTextVisibility(String text, boolean visible) {
        onView(allOf(withId(android.R.id.summary), hasSibling(withText(text))))
                .check(matches(visible ? isDisplayed() : not(isDisplayed())));
    }

    static View getRootViewSanitized(int text) {
        View[] view = {null};
        onViewWaiting(withText(text)).check((v, e) -> view[0] = v.getRootView());
        ThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    private void scrollToPreference(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private void scrollToExpandedPreference(String text) {
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        anyOf(
                                                allOf(
                                                        withId(R.id.buttons_container),
                                                        hasSibling(
                                                                withChild(
                                                                        withChild(
                                                                                withText(text))))),
                                                allOf(
                                                        withId(android.R.id.summary),
                                                        hasSibling(withText(text)))))));
    }

    private void scrollToLastPosition() {
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
    }

    private void setCompromisedPasswordsCount(int count) {
        // TODO(crbug.com/324562205): Add more integration tests for password module.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                });
    }

    private void addCredentialToAccountStore() {
        // Set up an account with at least one password in the account store.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        PasswordManagerTestHelper.setAccountForPasswordStore(SigninTestRule.TEST_ACCOUNT_EMAIL);

        PasswordStoreBridge passwordStoreBridge =
                ThreadUtils.runOnUiThreadBlocking(() -> new PasswordStoreBridge(mProfile));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    passwordStoreBridge.insertPasswordCredentialInAccountStore(
                            new PasswordStoreCredential(
                                    new GURL("https://site2.com"), "user2", "pwd2"));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The account store should've had one password",
                            passwordStoreBridge.getPasswordStoreCredentialsCountForAccountStore(),
                            is(1));
                });
    }

    private void setSafeBrowsingState(@SafeBrowsingState int state) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                            .setSafeBrowsingState(state);
                });
    }
}
