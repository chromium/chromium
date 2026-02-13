// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Build;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.WindowAndroid;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DisableFeatures({ChromeFeatureList.SETTINGS_MULTI_COLUMN})
@EnableFeatures({PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_LOUD})
@Batch(Batch.PER_CLASS)
public class PermissionClapperLoudTest {

    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();
    private static final String PAGE_URL = "/content/test/data/android/permission_navigation.html";

    private HistogramWatcher expectAction(@PermissionTestRule.PromptAction int action) {
        return HistogramWatcher.newBuilder()
                .expectIntRecordTimes(
                        "Permissions.Prompt.Notifications.MessageUILoud.Action", action, 1)
                .expectIntRecordTimes("Permissions.Action.WithDisposition.MessageUILoud", action, 1)
                .build();
    }

    private HistogramWatcher expectNoActions() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords("Permissions.Prompt.Notifications.MessageUILoud.Action")
                .expectNoRecords("Permissions.Action.WithDisposition.MessageUILoud")
                .build();
    }

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
        safelyClosePageInfo();
        clearPermissionDelegate();
        mPermissionRule.resetNotificationsSettingsForTest(/* enableQuietUi= */ false);
    }

    private void safelyClosePageInfo() {
        try {
            onView(withId(R.id.page_info_url_wrapper)).check(matches(isDisplayed()));
            // If we get here, it is displayed.
            pressBack();
            mPermissionRule.waitForPageInfoClose();
        } catch (NoMatchingViewException | AssertionError e) {
            // Not displayed, safe to proceed.
        }
    }

    private void triggerLoudClapper() throws Exception {
        mPermissionRule.setupPageAndTriggerNotificationPermissionRequest(PAGE_URL);
        mPermissionRule.waitForMessageShownState(true);
    }

    /**
     * Injects a mock Android permission delegate to simulate actions on OS-level permission
     * prompts.
     *
     * @param permissions Array of permissions to manage.
     * @param response The response to provide when permissions are requested.
     */
    private void injectPermissionDelegate(
            String[] permissions, RuntimePermissionTestUtils.RuntimePromptResponse response) {
        RuntimePermissionTestUtils.TestAndroidPermissionDelegate testDelegate =
                new RuntimePermissionTestUtils.TestAndroidPermissionDelegate(permissions, response);
        WindowAndroid windowAndroid = mPermissionRule.getActivity().getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    windowAndroid.setAndroidPermissionDelegate(testDelegate);
                });
    }

    /** Clears the injected Android permission delegate. */
    private void clearPermissionDelegate() {
        WindowAndroid windowAndroid = mPermissionRule.getActivity().getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    windowAndroid.setAndroidPermissionDelegate(null);
                });
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
    public void testLoudClapperAllow_OsGranted() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Allow", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Manage")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectBooleanRecord(
                                "Permissions.ClapperLoud.PageInfo.OsPromptResolved", true)
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .expectBooleanRecord("Permissions.ClapperLoud.PageInfo.Subscribed", true)
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.GRANTED);

        triggerLoudClapper();

        injectPermissionDelegate(
                new String[] {"android.permission.POST_NOTIFICATIONS"},
                RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_ALLOW_BUTTON_ID)).perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);

        mPermissionRule.waitForMessageShownState(false);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_ALLOWED_ID);

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
    public void testLoudClapperAllow_OsDenied() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Allow", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Manage")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectBooleanRecord(
                                "Permissions.ClapperLoud.PageInfo.OsPromptResolved", false)
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .build();

        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DISMISSED);

        triggerLoudClapper();

        injectPermissionDelegate(
                new String[] {"android.permission.POST_NOTIFICATIONS"},
                RuntimePermissionTestUtils.RuntimePromptResponse.DENY);

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_ALLOW_BUTTON_ID)).perform(click());

        mPermissionRule.waitForMessageShownState(false);

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testLoudClapperMenuDontAllow() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Deny", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Manage")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.OsPromptResolved")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);

        triggerLoudClapper();

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_DONT_ALLOW_BUTTON_TEXT_ID))
                .perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();
        mPermissionRule.waitForMessageShownState(false);

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_BLOCKED_ID);

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testLoudClapperDismiss() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Dismiss", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Deny")
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Manage")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.OsPromptResolved")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);

        triggerLoudClapper();

        onViewWaiting(withId(R.id.message_banner)).perform(swipeUp());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();
        mPermissionRule.waitForMessageShownState(false);

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
    public void testLoudClapperManage_Subscribe_OsGranted() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Manage", true)
                        .expectBooleanRecord("Permissions.ClapperLoud.PageInfo.Subscribed", true)
                        .expectBooleanRecord(
                                "Permissions.ClapperLoud.PageInfo.OsPromptResolved", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.GRANTED);

        triggerLoudClapper();

        injectPermissionDelegate(
                new String[] {"android.permission.POST_NOTIFICATIONS"},
                RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPageInfoOpen();

        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID))
                .perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        pressBack();
        mPermissionRule.waitForPageInfoClose();

        mPermissionRule.waitForStatusIconGone(PermissionTestRule.NOTIFICATIONS_ALLOWED_ID);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.TIRAMISU)
    public void testLoudClapperManage_Subscribe_OsDenied() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Manage", true)
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .expectBooleanRecord(
                                "Permissions.ClapperLoud.PageInfo.OsPromptResolved", false)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DISMISSED);

        triggerLoudClapper();

        injectPermissionDelegate(
                new String[] {"android.permission.POST_NOTIFICATIONS"},
                RuntimePermissionTestUtils.RuntimePromptResponse.DENY);

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPageInfoOpen();

        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID))
                .perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        onViewWaiting(withText(R.string.page_info_permissions_title));

        onViewWaiting(withId(R.id.subpage_back_button)).perform(click());

        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testLoudClapperManage_Reset() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Manage", true)
                        .expectBooleanRecord("Permissions.ClapperLoud.PageInfo.Reset", true)
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Closed")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.OsPromptResolved")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DISMISSED);

        triggerLoudClapper();

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPageInfoOpen();

        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID))
                .perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_DIALOG_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testLoudClapperManage_Reset_Cancel() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Manage", true)
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .build();
        HistogramWatcher actionWatcher = expectNoActions();

        triggerLoudClapper();

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPageInfoOpen();

        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID))
                .perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_CANCEL_DIALOG_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        onViewWaiting(withText(R.string.page_info_permissions_title));

        actionWatcher.assertExpected();
        actionWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);
        onViewWaiting(withId(R.id.subpage_back_button)).perform(click());
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_BLOCKED_ID);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();

        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testLoudClapperManage_Back() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Permissions.ClapperLoud.MessageUI.Manage", true)
                        .expectBooleanRecord("Permissions.ClapperLoud.PageInfo.Closed", true)
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Reset")
                        .expectNoRecords("Permissions.ClapperLoud.MessageUI.Allow")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.OsPromptResolved")
                        .expectNoRecords("Permissions.ClapperLoud.PageInfo.Subscribed")
                        .build();
        HistogramWatcher actionWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);

        triggerLoudClapper();

        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID)).perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT))
                .perform(click());

        mPermissionRule.waitForPageInfoOpen();

        pressBack();
        mPermissionRule.waitForPageInfoClose();

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();
        actionWatcher.assertExpected();
    }
}
