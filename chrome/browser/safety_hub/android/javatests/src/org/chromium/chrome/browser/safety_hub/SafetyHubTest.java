// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

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

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests for various Safety Hub settings surfaces. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
@Batch(Batch.PER_CLASS)
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

    private FakeUnusedSitePermissionsBridge mUnusedPermissionsBridge =
            new FakeUnusedSitePermissionsBridge();

    private FakeNotificationPermissionReviewBridge mNotificationPermissionReviewBridge =
            new FakeNotificationPermissionReviewBridge();

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

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_1.getOrigin());
        onView(withText(PERMISSIONS_DATA_1.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_1.getOrigin())).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testClearPermissionsReviewList() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        mSafetyHubFragmentTestRule.startSettingsActivity();

        // Verify the permissions module is displaying the warning state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 2, 2);
        scrollToPreference(withText(permissionsTitle));
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in a warning state.
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

        // Click on the snackbar action button and verify that the warning is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(permissionsTitle)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubPermissions"})
    public void testPermissionsToSiteSettings() {
        SettingsActivity activity = mPermissionsFragmentTestRule.startSettingsActivity();
        openActionBarOverflowOrOptionsMenu(activity);
        onViewWaiting(withText(R.string.safety_hub_permissions_page_menu_text)).perform(click());
        onViewWaiting(withText(R.string.prefs_site_settings)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationBlock() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mNotificationsFragmentTestRule.startSettingsActivity();

        // Block the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_block_notifications_menu_item)).perform(click());
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationAllow() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mNotificationsFragmentTestRule.startSettingsActivity();

        // Always allow the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_allow_notifications_menu_item)).perform(click());
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationAsk() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {NOTIFICATION_PERMISSIONS_1});
        mNotificationsFragmentTestRule.startSettingsActivity();

        // Reset the notification by clicking the corresponding menu button.
        clickOnButtonNextToText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern());
        onViewWaiting(withText(R.string.safety_hub_ask_notifications_menu_item)).perform(click());
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testBlockAllNotifications() {
        mNotificationPermissionReviewBridge.setNotificationPermissionsForReview(
                new NotificationPermissions[] {
                    NOTIFICATION_PERMISSIONS_1, NOTIFICATION_PERMISSIONS_2
                });
        mSafetyHubFragmentTestRule.startSettingsActivity();

        // Verify the notifications module is displaying the warning state.
        String notificationsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title, 2, 2);
        scrollToPreference(withText(notificationsTitle));
        onView(withText(notificationsTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in a warning state.
        verifyButtonsNextToTextVisibility(notificationsTitle, true);

        // Open the notifications subpage.
        clickOnSecondaryButtonNextToText(notificationsTitle);

        // Verify that 2 sites are displayed.
        onView(withText(NOTIFICATION_PERMISSIONS_1.getPrimaryPattern()))
                .check(matches(isDisplayed()));
        onView(withText(NOTIFICATION_PERMISSIONS_2.getPrimaryPattern()))
                .check(matches(isDisplayed()));

        // Click the button at the bottom of the page.
        onView(withText(R.string.safety_hub_notifications_block_all_button)).perform(click());

        // Verify tha the notifications subpage has been dismissed and the state of the
        // notification module has changed.
        onViewWaiting(withText(R.string.safety_hub_notifications_review_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the warning is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(notificationsTitle)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"SafetyHubNotifications"})
    public void testNotificationsToNotificationSettings() {
        SettingsActivity activity = mNotificationsFragmentTestRule.startSettingsActivity();
        openActionBarOverflowOrOptionsMenu(activity);
        onViewWaiting(withText(R.string.safety_hub_notifications_page_menu_text)).perform(click());
        onViewWaiting(
                        allOf(
                                withText(R.string.push_notifications_permission_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubSafeBrowsing"})
    public void testSafeBrowsingModule() {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                                .setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION));

        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify the safe browsing module is displaying the enhanced protection state.
        String safeBrowsingTitle =
                safetyHubFragment.getString(R.string.safety_hub_safe_browsing_enhanced_title);
        scrollToPreference(withText(safeBrowsingTitle));
        onView(withText(safeBrowsingTitle)).check(matches(isDisplayed()));

        // Module should be collapsed initially since it's in a safe state.
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, false);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Expand the module to show the buttons.
        clickOnExpandButtonNextToText(safeBrowsingTitle);

        // Open the Safe Browsing settings.
        clickOnSecondaryButtonNextToText(safeBrowsingTitle);

        onViewWaiting(withText(R.string.prefs_safe_browsing_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"SafetyHubUpdateCheck"})
    @CommandLineFlags.Add({
        ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE + "=update_available",
    })
    public void testUpdateCheckModule() {
        // TODO(crbug.com/324562205): Move the initialization of the SafetyHubFetchService so that
        // there is no dependency on ChromeTabbedActivity.
        mSafetyHubFragmentTestRule.startSettingsActivity();
        SafetyHubFragment safetyHubFragment = mSafetyHubFragmentTestRule.getFragment();

        // Verify the update check module is displaying the "Update available" state.
        String updateCheckTitle =
                safetyHubFragment.getString(R.string.safety_check_updates_outdated);
        scrollToPreference(withText(updateCheckTitle));
        onView(withText(updateCheckTitle)).check(matches(isDisplayed()));

        // Module should be expanded initially since it's in a warning state.
        verifyButtonsNextToTextVisibility(updateCheckTitle, true);

        if (BuildConfig.IS_CHROME_BRANDED) {
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
        }
    }

    @Test
    @MediumTest
    public void testPreferenceExpand() {
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        new SafeBrowsingBridge(ProfileManager.getLastUsedRegularProfile())
                                .setSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING));

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
        clickOnExpandButtonNextToText(safeBrowsingTitle);
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, false);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, false);

        // Click on expand button.
        clickOnExpandButtonNextToText(safeBrowsingTitle);
        verifyButtonsNextToTextVisibility(safeBrowsingTitle, true);
        verifySummaryNextToTextVisibility(safeBrowsingTitle, true);
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

    private void clickOnExpandButtonNextToText(String text) {
        onViewWaiting(
                        allOf(
                                withId(R.id.checkable_image_view),
                                hasSibling(withChild(withText(text)))))
                .perform(click());
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
        onViewWaiting(withText(text)).check(((v, e) -> view[0] = v.getRootView()));
        TestThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view[0]));
        return view[0];
    }

    private void scrollToPreference(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }
}
