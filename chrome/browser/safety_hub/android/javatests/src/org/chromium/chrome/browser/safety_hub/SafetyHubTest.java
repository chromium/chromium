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

import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.DASHBOARD_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.NOTIFICATIONS_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils.PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
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
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.test.util.TestAccounts;
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
@Features.DisableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.SETTINGS_MULTI_COLUMN
})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DisableIf.Build(
        sdk_equals = Build.VERSION_CODES.Q,
        message = "crbug.com/447426928, crashing emulator with --disable-field-trial-config")
@DoNotBatch(reason = "Manages sign-in state, which is global.")
public final class SafetyHubTest {
    // This test suite currently expects that calls to password check via PasswordManagerHelper
    // cause an exception, so the state of the UI can be controlled by setting prefs in
    // setAccountCompromisedPasswordsCount() and friends.
    private static class FailingPasswordCheckupClientHelper implements PasswordCheckupClientHelper {
        @Override
        public void getPasswordCheckupIntent(
                int referrer,
                @Nullable String accountName,
                Callback<PendingIntent> successCallback,
                Callback<Exception> failureCallback) {
            failureCallback.onResult(new Exception("error"));
        }

        @Override
        public void runPasswordCheckupInBackground(
                int referrer,
                @Nullable String accountName,
                Callback<Void> successCallback,
                Callback<Exception> failureCallback) {
            failureCallback.onResult(new Exception("error"));
        }

        @Override
        public void getBreachedCredentialsCount(
                int referrer,
                @Nullable String accountName,
                Callback<Integer> successCallback,
                Callback<Exception> failureCallback) {
            failureCallback.onResult(new Exception("error"));
        }

        @Override
        public void getWeakCredentialsCount(
                int referrer,
                @Nullable String accountName,
                Callback<Integer> successCallback,
                Callback<Exception> failureCallback) {
            failureCallback.onResult(new Exception("error"));
        }

        @Override
        public void getReusedCredentialsCount(
                int referrer,
                @Nullable String accountName,
                Callback<Integer> successCallback,
                Callback<Exception> failureCallback) {
            failureCallback.onResult(new Exception("error"));
        }
    }

    private static final PermissionsData PERMISSIONS_DATA_1 =
            PermissionsData.create(
                    "http://example1.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingsType.MEDIASTREAM_MIC
                    },
                    0,
                    0,
                    PermissionsRevocationType.UNUSED_PERMISSIONS);

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
                    0,
                    PermissionsRevocationType.UNUSED_PERMISSIONS);

    private static final PermissionsData PERMISSIONS_DATA_3 =
            PermissionsData.create(
                    "http://example3.com",
                    new int[] {ContentSettingsType.NOTIFICATIONS, ContentSettingsType.GEOLOCATION},
                    0,
                    0,
                    PermissionsRevocationType.UNUSED_PERMISSIONS_AND_ABUSIVE_NOTIFICATIONS);
    private static final PermissionsData PERMISSIONS_DATA_4 =
            PermissionsData.create(
                    "http://example4.com",
                    new int[] {ContentSettingsType.NOTIFICATIONS},
                    0,
                    0,
                    PermissionsRevocationType.DISRUPTIVE_NOTIFICATION_PERMISSIONS);
    private static final PermissionsData PERMISSIONS_DATA_5 =
            PermissionsData.create(
                    "http://example5.com",
                    new int[] {ContentSettingsType.NOTIFICATIONS},
                    0,
                    0,
                    PermissionsRevocationType.SUSPICIOUS_NOTIFICATION_PERMISSIONS);
    private static final NotificationPermissions NOTIFICATION_PERMISSIONS_1 =
            NotificationPermissions.create("http://example1.com", "*", 3);
    private static final NotificationPermissions NOTIFICATION_PERMISSIONS_2 =
            NotificationPermissions.create("http://example2.com", "*", 8);

    private static final String PREF_NOTIFICATIONS_REVIEW = "notifications_review";
    private static final int RENDER_TEST_REVISION = 2;

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

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .setRevision(RENDER_TEST_REVISION)
                    .build();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private final FakeUnusedSitePermissionsBridge mUnusedPermissionsBridge =
            new FakeUnusedSitePermissionsBridge();

    private final FakeNotificationPermissionReviewBridge mNotificationPermissionReviewBridge =
            new FakeNotificationPermissionReviewBridge();

    private WebPageStation mPage;
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
        UnusedSitePermissionsBridgeJni.setInstanceForTesting(mUnusedPermissionsBridge);
        NotificationPermissionReviewBridgeJni.setInstanceForTesting(
                mNotificationPermissionReviewBridge);

        mPage = mActivityTestRule.startOnBlankPage();
        mProfile = mActivityTestRule.getProfile(/* incognito= */ false);

        PasswordCheckupClientHelper helper = new FailingPasswordCheckupClientHelper();
        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                new PasswordCheckupClientHelperFactory() {
                    @Override
                    public PasswordCheckupClientHelper createHelper() {
                        return helper;
                    }
                });

        // Reset state to the default of the compromised passwords count and the browsing data
        // state.
        clearAccountCompromisedPasswordsCount();
        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(
            ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION + ":shadow_run/false")
    public void testFragmentAppearanceDisruptiveRevocation() throws IOException {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubExpandablePreference preference =
                mSafetyHubFragmentTestRule.getFragment().findPreference(PREF_NOTIFICATIONS_REVIEW);
        Assert.assertFalse(preference.isVisible());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(
            ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION + ":shadow_run/true")
    public void testFragmentAppearanceShadowRun() throws IOException {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubExpandablePreference preference =
                mSafetyHubFragmentTestRule.getFragment().findPreference(PREF_NOTIFICATIONS_REVIEW);
        Assert.assertTrue(preference.isVisible());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest", "SafetyHubPermissions"})
    @Features.DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testPermissionsSubpageAppearance() throws IOException {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        mPermissionsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getRootViewSanitized(R.string.safety_hub_permissions_page_title),
                "permissions_subpage");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest", "SafetyHubPermissions"})
    @Features.DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testNotificationPermissionsSubpageAppearance() throws IOException {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_3, PERMISSIONS_DATA_4, PERMISSIONS_DATA_5});
        mPermissionsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getRootViewSanitized(R.string.safety_hub_permissions_page_title),
                "notification_permissions_subpage");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest", "SafetyHubNotifications"})
    @Features.DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
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
                        .expectNoRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME)
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
                        .expectNoRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME)
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

        // Module should be expanded initially since it's in an info state.
        verifyButtonsNextToTextVisibility(permissionsTitle, true);

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
    // Disruptive notification revocation disables the notification review module.
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
    @DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/382238797")
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

        // Verify that the notifications subpage has been dismissed and the state of the
        // notification module has changed.
        String okNotificationTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getString(R.string.safety_hub_notifications_review_ok_title);
        scrollToExpandedPreference(okNotificationTitle);
        onViewWaiting(withText(okNotificationTitle)).check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the info state is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(notificationsTitle)).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    // Disruptive notification revocation disables the notification review module.
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
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
    @DisabledTest(message = "https://crbug.com/411312866")
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
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Click on collapse button.
        expandPreferenceWithText(safeBrowsingTitle);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Click on expand button.
        expandPreferenceWithText(safeBrowsingTitle);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Reset Safe Browsing state so it doesn't leak to other tests.
        setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @RequiresRestart
    // Disruptive notification revocation disables the notification review module.
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
    public void testMultiplePreferenceExpand() {
        // Set a module with an unmanaged warning state.
        int compromisedPasswordsCount = 5;
        addCredentialToAccountStore();
        setAccountCompromisedPasswordsCount(compromisedPasswordsCount);

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
                                R.plurals.safety_hub_account_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        scrollToExpandedPreference(passwordsTitle);
        verifyButtonsNextToTextVisibility(passwordsTitle, true);

        // Verify other modules are collapsed.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        String notificationsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 2, 2);
        scrollToPreference(withText(notificationsTitle));
        verifyButtonsNextToTextVisibility(notificationsTitle, false);

        // Fix the warning state
        setAccountCompromisedPasswordsCount(0);
        mSafetyHubFragmentTestRule.recreateActivity();
        safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        passwordsTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_no_compromised_account_passwords_title);
        scrollToPreference(withText(passwordsTitle));
        verifyButtonsNextToTextVisibility(passwordsTitle, false);

        // Verify info modules are now expanded.
        scrollToExpandedPreference(safeBrowsingTitle);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        scrollToExpandedPreference(notificationsTitle);
        verifyButtonsNextToTextVisibility(notificationsTitle, true);

        // Make sure the compromised passwords count is reset at the end of the test.
        clearAccountCompromisedPasswordsCount();
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void testAccountPasswordModule_WeakPasswords() {
        // Set the passwords module to the information state.
        int weakPasswordsCount = 5;
        addCredentialToAccountStore();
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(5);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that passwords module which is in the information state is expanded by default.
        String weakPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakPasswordsCount,
                                weakPasswordsCount);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Make the password module state be warning.
        int compromisedPasswordsCount = 5;
        setAccountCompromisedPasswordsCount(compromisedPasswordsCount);
        mSafetyHubFragmentTestRule.recreateActivity();
        safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that the password module is expanded, since it's on the warning state.
        String compromisedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        scrollToExpandedPreference(compromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(compromisedPasswordsTitle, true);

        // Verify that the other module in the information state is now collapsed.
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Make sure the compromised passwords count is reset at the end of the test.
        clearAccountCompromisedPasswordsCount();
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void testAccountPasswordModule_CompromisedCountUnavailable_WeakAndReusedPasswords() {
        // Set the passwords module to the unavailable state.
        addCredentialToAccountStore();
        setAccountCompromisedPasswordsCount(-1);
        setAccountWeakPasswordsCount(0);
        setAccountReusedPasswordsCount(0);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that passwords module which is in the unavailable state is expanded by
        // default.
        String noWeakAndReusedPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_reused_weak_passwords_title);
        scrollToExpandedPreference(noWeakAndReusedPasswordsTitle);
        verifyButtonsNextToTextVisibility(noWeakAndReusedPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Set weak and reused passwords to unavailable.
        setAccountWeakPasswordsCount(-1);
        setAccountReusedPasswordsCount(-1);
        mSafetyHubFragmentTestRule.recreateActivity();
        safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that the password module is still expanded, but now with the unavailable title.
        String weakPasswordsTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_account_password_check_unavailable_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifySummaryNextToTextVisibility(weakPasswordsTitle, true);

        // Verify that the other module in the information state is still expanded.
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void testAccountPasswordModule_WeakAndReusedPasswords() {
        // Set the passwords module to the information state.
        int weakPasswordsCount = 5;
        int reusedPasswordsCount = 4;
        addCredentialToAccountStore();
        setAccountCompromisedPasswordsCount(0);
        setAccountWeakPasswordsCount(weakPasswordsCount);
        setAccountReusedPasswordsCount(reusedPasswordsCount);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that passwords module which is in the information state and is expanded by
        // default.
        // Reused passwords are prioritized over weak passwords.
        String reusedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                reusedPasswordsCount,
                                reusedPasswordsCount);
        scrollToExpandedPreference(reusedPasswordsTitle);
        verifyButtonsNextToTextVisibility(reusedPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        // Set reused passwords to 0.
        setAccountReusedPasswordsCount(0);
        mSafetyHubFragmentTestRule.recreateActivity();
        safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that the password module is still expanded, but now with the weak passwords title.
        String weakPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakPasswordsCount,
                                weakPasswordsCount);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify that the other module in the information state is still expanded.
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_AllCountsUnavailable() {
        setLocalCompromisedPasswordsCount(-1);
        setLocalWeakPasswordsCount(-1);
        setLocalReusedPasswordsCount(-1);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the unavailable state is expanded by
        // default.
        String unavailableTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_local_password_check_unavailable_title);
        scrollToExpandedPreference(unavailableTitle);
        verifyButtonsNextToTextVisibility(unavailableTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_AllCountsUnavailable_NotWithinCoolDown() {
        setLocalCompromisedPasswordsCount(1);
        setLocalWeakPasswordsCount(1);
        setLocalReusedPasswordsCount(1);
        setLocalPasswordCheckTimestamp(0);
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the unavailable state is expanded by
        // default.
        String unavailableTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_local_password_check_unavailable_title);
        scrollToExpandedPreference(unavailableTitle);
        verifyButtonsNextToTextVisibility(unavailableTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_NoPasswords() {
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the info state is expanded by
        // default.
        String noPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_local_passwords_title);
        scrollToExpandedPreference(noPasswordsTitle);
        verifyButtonsNextToTextVisibility(noPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_HasCompromisedPasswords() {
        // Set the local passwords module to the warning state.
        int compromisedPasswordsCount = 5;
        setLocalCompromisedPasswordsCount(compromisedPasswordsCount);
        setLocalWeakPasswordsCount(2);
        setLocalReusedPasswordsCount(1);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the warning state is expanded by
        // default.
        String compromisedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        scrollToExpandedPreference(compromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(compromisedPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_HasReusedAndWeakPasswords() {
        // Set the local passwords module to the info state.
        setLocalCompromisedPasswordsCount(0);
        setLocalWeakPasswordsCount(2);
        setLocalReusedPasswordsCount(1);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the info state is expanded by
        // default.
        String weakAndReusedPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        scrollToExpandedPreference(weakAndReusedPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakAndReusedPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_HasWeakPasswords() {
        // Set the local passwords module to the info state.
        setLocalCompromisedPasswordsCount(0);
        setLocalWeakPasswordsCount(2);
        setLocalReusedPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the info state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordModule_NoCompromisedPasswords() {
        // Set the local passwords module to the safe state.
        setLocalCompromisedPasswordsCount(0);
        setLocalWeakPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that local passwords module which is in the safe state is not expanded by
        // default.
        String noCompromisedPasswordsTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_no_compromised_local_passwords_title);
        scrollToExpandedPreference(noCompromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(noCompromisedPasswordsTitle, false);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountAndLocalCompromisedPasswords() {
        int accountCompromisedPasswordsCount = 2;
        int localCompromisedPasswordsCount = 3;
        setAccountCompromisedPasswordsCount(accountCompromisedPasswordsCount);
        setLocalCompromisedPasswordsCount(localCompromisedPasswordsCount);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Compromised passwords take precedent over reused or weak.
        setAccountReusedPasswordsCount(1);
        setLocalReusedPasswordsCount(1);
        setAccountWeakPasswordsCount(1);
        setLocalWeakPasswordsCount(1);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        int totalCompromisedPasswordsCount =
                accountCompromisedPasswordsCount + localCompromisedPasswordsCount;
        String compromisedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_passwords_compromised_title,
                                totalCompromisedPasswordsCount,
                                totalCompromisedPasswordsCount);

        // Wait for the loading to disappear and the final state to be shown.
        onViewWaiting(withText(compromisedPasswordsTitle)).check(matches(isDisplayed()));

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        scrollToExpandedPreference(compromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(compromisedPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountCompromisedPasswords() {
        int accountCompromisedPasswordsCount = 2;
        setAccountCompromisedPasswordsCount(accountCompromisedPasswordsCount);
        setLocalCompromisedPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Compromised passwords take precedent over reused or weak.
        setAccountReusedPasswordsCount(1);
        setLocalReusedPasswordsCount(1);
        setAccountWeakPasswordsCount(1);
        setLocalWeakPasswordsCount(1);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        String compromisedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
                                accountCompromisedPasswordsCount,
                                accountCompromisedPasswordsCount);
        scrollToExpandedPreference(compromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(compromisedPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_LocalCompromisedPasswords() {
        int localCompromisedPasswordsCount = 3;
        setAccountCompromisedPasswordsCount(0);
        setLocalCompromisedPasswordsCount(localCompromisedPasswordsCount);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Compromised passwords take precedent over reused or weak.
        setAccountReusedPasswordsCount(1);
        setLocalReusedPasswordsCount(1);
        setAccountWeakPasswordsCount(1);
        setLocalWeakPasswordsCount(1);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        String compromisedPasswordsTitle =
                safetyHubFragment
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                localCompromisedPasswordsCount,
                                localCompromisedPasswordsCount);
        scrollToExpandedPreference(compromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(compromisedPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountAndLocalReusedPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(1);
        setLocalReusedPasswordsCount(2);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Reused passwords take precedent over weak.
        setAccountWeakPasswordsCount(2);
        setLocalWeakPasswordsCount(3);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the info state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountReusedPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(2);
        setLocalReusedPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Reused passwords take precedent over weak.
        setAccountWeakPasswordsCount(2);
        setLocalWeakPasswordsCount(3);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the info state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_reused_weak_account_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_LocalReusedPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setLocalReusedPasswordsCount(2);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        // Reused passwords take precedent over weak.
        setAccountWeakPasswordsCount(2);
        setLocalWeakPasswordsCount(3);

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountAndLocalWeakPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(2);
        setLocalWeakPasswordsCount(3);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the info state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountWeakPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(2);
        setLocalWeakPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the info state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(
                        R.string.safety_hub_reused_weak_account_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_LocalWeakPasswords() {
        setLocalCompromisedPasswordsCount(0);
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(0);
        setLocalWeakPasswordsCount(3);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the warning state is expanded by
        // default.
        String weakPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        scrollToExpandedPreference(weakPasswordsTitle);
        verifyButtonsNextToTextVisibility(weakPasswordsTitle, true);

        // Verify the information module is not expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_NoAccountAndLocalCompromisedPasswords() {
        setAccountCompromisedPasswordsCount(0);
        setLocalCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(0);
        setLocalWeakPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the safe state is collapsed by default.
        String noCompromisedPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_compromised_passwords_title);
        scrollToExpandedPreference(noCompromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(noCompromisedPasswordsTitle, false);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_NoAccountCompromisedPasswords_NoLocalPasswords() {
        clearAllPasswords();
        setAccountCompromisedPasswordsCount(0);
        setAccountReusedPasswordsCount(0);
        setAccountWeakPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToAccountStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the safe state is collapsed by default.
        String noCompromisedPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_compromised_passwords_title);
        scrollToExpandedPreference(noCompromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(noCompromisedPasswordsTitle, false);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_NoLocalCompromisedPasswords_NoAccountPasswords() {
        signIn();
        clearAllPasswords();
        setLocalCompromisedPasswordsCount(0);
        setLocalReusedPasswordsCount(0);
        setLocalWeakPasswordsCount(0);
        setLocalPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        setAccountPasswordCheckTimestamp(TimeUtils.currentTimeMillis());
        addCredentialToProfileStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify that unified passwords module which is in the safe state is collapsed by default.
        String noCompromisedPasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_no_compromised_passwords_title);
        scrollToExpandedPreference(noCompromisedPasswordsTitle);
        verifyButtonsNextToTextVisibility(noCompromisedPasswordsTitle, false);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_AccountAndLocalPasswordsUnavailable() {
        clearLocalCompromisedPasswordsCount();
        clearAccountCompromisedPasswordsCount();
        addCredentialToProfileStore();
        addCredentialToAccountStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        String unavailablePasswordsTitle =
                safetyHubFragment.getString(R.string.safety_hub_password_check_unavailable_title);

        // Wait for the loading to disappear and the final state to be shown.
        onViewWaiting(withText(unavailablePasswordsTitle)).check(matches(isDisplayed()));

        // Verify that unified passwords module which is in the unavailable state is expanded by
        // default.
        scrollToExpandedPreference(unavailablePasswordsTitle);
        verifyButtonsNextToTextVisibility(unavailablePasswordsTitle, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_NoAccountAndLocalPasswords() {
        signIn();
        clearAllPasswords();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        String noAccountAndLocalPasswords =
                safetyHubFragment.getString(R.string.safety_hub_no_passwords_title);

        // Wait for the loading to disappear and the final state to be shown.
        onViewWaiting(withText(noAccountAndLocalPasswords)).check(matches(isDisplayed()));

        // Verify that unified passwords module which is in the info state is expanded by
        // default.
        scrollToExpandedPreference(noAccountAndLocalPasswords);
        verifyButtonsNextToTextVisibility(noAccountAndLocalPasswords, true);

        // Verify the information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "SafeBrowsingEnabled", string = "false")})
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
        ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
    })
    public void testUnifiedPasswordsModule_CountsUnavailable_NotWithinCoolDown() {
        setLocalCompromisedPasswordsCount(1);
        setLocalWeakPasswordsCount(1);
        setLocalReusedPasswordsCount(1);
        setAccountCompromisedPasswordsCount(1);
        setAccountWeakPasswordsCount(1);
        setAccountReusedPasswordsCount(1);
        setLocalPasswordCheckTimestamp(0);
        setAccountPasswordCheckTimestamp(0);
        addCredentialToProfileStore();
        addCredentialToAccountStore();

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        String unavailableTitle =
                safetyHubFragment.getString(R.string.safety_hub_password_check_unavailable_title);

        // Wait for the loading to disappear and the final state to be shown.
        onViewWaiting(withText(unavailableTitle)).check(matches(isDisplayed()));

        // Verify that unified passwords module which is in the unavailable state is expanded by
        // default.
        scrollToExpandedPreference(unavailableTitle);
        verifyButtonsNextToTextVisibility(unavailableTitle, true);

        // Verify the other information module is expanded.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.prefs_safe_browsing_no_protection_summary);
        scrollToPreference(withText(safeBrowsingTitle));
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);

        clearLocalCompromisedPasswordsCount();
        setLocalPasswordCheckTimestamp(0);
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
                        .expectNoRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME)
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
    // Disruptive notification revocation disables the notification review module.
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
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

        // Click on the reset all button.
        clickOnPrimaryButtonNextToText(notificationsTitle);

        // Verify the notification module has changed to a safe state.
        String okNotificationsTitle =
                safetyHubFragment.getString(R.string.safety_hub_notifications_review_ok_title);
        scrollToExpandedPreference(okNotificationsTitle);
        onViewWaiting(withText(okNotificationsTitle)).check(matches(isDisplayed()));

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
    // Disruptive notification revocation disables the notification review module.
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
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
    public void testSafetyToolsLearnMoreLink_OpensInCct() {
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
    public void testIncognitoLearnMoreLink_OpensInCct() {
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
    public void testSafeBrowsingLearnMoreLink_OpensInCct() {
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

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testAbusiveNotificationPermissionRegrant() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_3});
        mPermissionsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .build();

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_3.getOrigin());
        onView(withText(PERMISSIONS_DATA_3.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_3.getOrigin())).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testClearAbusiveNotificationPermissionsReviewList() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_3});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ACKNOWLEDGE_ALL,
                                PermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL)
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
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 1, 1);
        scrollToExpandedPreference(permissionsTitle);
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in an info state and there are no other
        // warning states.
        verifyButtonsNextToTextVisibility(permissionsTitle, true);

        // Open the permissions subpage.
        clickOnSecondaryButtonNextToText(permissionsTitle);

        // Verify that the site is displayed.
        onView(withText(PERMISSIONS_DATA_3.getOrigin())).check(matches(isDisplayed()));

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
    public void testDisruptiveNotificationPermissionRegrant() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_4});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        mPermissionsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .expectNoRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME)
                        .build();

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_4.getOrigin());
        onView(withText(PERMISSIONS_DATA_4.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_4.getOrigin())).check(matches(isDisplayed()));

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testSuspiciousNotificationPermissionRegrant() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_5});
        mSafetyHubFragmentTestRule.startSettingsActivity();
        mPermissionsFragmentTestRule.startSettingsActivity();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                ABUSIVE_NOTIFICATION_REVOCATION_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .expectIntRecords(
                                PERMISSIONS_INTERACTIONS_HISTOGRAM_NAME,
                                PermissionsModuleInteractions.ALLOW_AGAIN,
                                PermissionsModuleInteractions.UNDO_ALLOW_AGAIN)
                        .build();

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_5.getOrigin());
        onView(withText(PERMISSIONS_DATA_5.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_5.getOrigin())).check(matches(isDisplayed()));

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
        Matcher<View> viewMatcher =
                allOf(
                        withId(R.id.buttons_container),
                        hasSibling(withChild(withChild(withText(text)))));
        if (visible) {
            onViewWaiting(allOf(viewMatcher, isDisplayed())).check(matches(isDisplayed()));
        } else {
            onView(viewMatcher).check(matches(not(isDisplayed())));
        }
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

    private void setAccountCompromisedPasswordsCount(int count) {
        // TODO(crbug.com/324562205): Add more integration tests for password module.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.BREACHED_CREDENTIALS_COUNT, count);
                });
    }

    private void clearAccountCompromisedPasswordsCount() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
                });
    }

    private void setAccountWeakPasswordsCount(int count) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.WEAK_CREDENTIALS_COUNT, count);
                });
    }

    private void setAccountReusedPasswordsCount(int count) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.REUSED_CREDENTIALS_COUNT, count);
                });
    }

    private void clearLocalCompromisedPasswordsCount() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).clearPref(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT);
                });
    }

    private void setLocalCompromisedPasswordsCount(int count) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile)
                            .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, count);
                });
    }

    private void setLocalWeakPasswordsCount(int count) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, count);
                });
    }

    private void setLocalPasswordCheckTimestamp(long timestampInMs) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile)
                            .setLong(
                                    Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED,
                                    timestampInMs);
                });
    }

    private void setAccountPasswordCheckTimestamp(long timestampInMs) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile)
                            .setLong(
                                    Pref.LAST_TIME_IN_MS_ACCOUNT_PASSWORD_CHECK_COMPLETED,
                                    timestampInMs);
                });
    }

    private void setLocalReusedPasswordsCount(int count) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(mProfile).setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, count);
                });
    }

    private void signIn() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        PasswordManagerTestHelper.setAccountForPasswordStore(TestAccounts.ACCOUNT1.getEmail());
    }

    private void addCredentialToAccountStore() {
        // Set up an account with at least one password in the account store.
        signIn();

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

    private void addCredentialToProfileStore() {
        PasswordStoreBridge passwordStoreBridge =
                ThreadUtils.runOnUiThreadBlocking(() -> new PasswordStoreBridge(mProfile));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    passwordStoreBridge.insertPasswordCredentialInProfileStore(
                            new PasswordStoreCredential(
                                    new GURL("https://site2.com"), "user2", "pwd2"));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The local store should've had one password.",
                            passwordStoreBridge.getPasswordStoreCredentialsCountForProfileStore(),
                            is(1));
                });
    }

    private void clearAllPasswords() {
        PasswordStoreBridge passwordStoreBridge =
                ThreadUtils.runOnUiThreadBlocking(() -> new PasswordStoreBridge(mProfile));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    passwordStoreBridge.clearAllPasswords();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The account store should've had any passwords",
                            passwordStoreBridge.getPasswordStoreCredentialsCountForAllStores(),
                            is(0));
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
