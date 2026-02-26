// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Tests for Gesture-Gated Permission Prompts.
 *
 * <p>This test suite verifies the behavior of permission prompts when the
 * "PermissionsGestureGatedPrompts" feature is enabled. TODO(crbug.com/4851977820): Look at android
 * automotive test failure
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors",
    "enable-features=PermissionsGestureGatedPrompts:mute_notifications/true/mute_geolocation/true"
})
@DisableFeatures({
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID
})
@EnableFeatures({
    PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION,
    PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_LOUD,
    PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_QUIET,
    PermissionsAndroidFeatureList.PERMISSIONS_GESTURE_GATED_PROMPTS
})
@Batch(Batch.PER_CLASS)
public class PermissionGestureGatedTest {

    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String PAGE_URL = "/content/test/data/android/permission_navigation.html";

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
        mPermissionRule.resetNotificationsSettingsForTest(/* enableQuietUi= */ false);
        mPermissionRule.setUpUrl(PAGE_URL);
        injectPermissionDelegate(
                new String[] {
                    "android.permission.POST_NOTIFICATIONS",
                    "android.permission.ACCESS_FINE_LOCATION",
                    "android.permission.ACCESS_COARSE_LOCATION"
                },
                RuntimePermissionTestUtils.RuntimePromptResponse.GRANT);
        waitForSecurityIcon();
    }

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

    private void waitForSecurityIcon() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    private void waitForQuietIcon() {
        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);
    }

    private void clickClapperQuietIcon() {
        mPermissionRule.openPageInfoFromStatusIcon();
    }

    private void triggerNotificationPermissionRequest(boolean withGesture) throws Exception {
        if (withGesture) {
            // Inject click handler.
            mPermissionRule.runJavaScriptCodeInCurrentTab(
                    "window.onclick = function() { if (window.functionToRun) {"
                            + " eval(window.functionToRun); } };");
            mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                    "Notification.requestPermission()");
        } else {
            mPermissionRule.runJavaScriptCodeInCurrentTab("Notification.requestPermission()");
        }
    }

    private void triggerGeolocationPermissionRequest(boolean withGesture) throws Exception {
        if (withGesture) {
            // Inject click handler.
            mPermissionRule.runJavaScriptCodeInCurrentTab(
                    "window.onclick = function() { if (window.functionToRun) {"
                            + " eval(window.functionToRun); } };");
            mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                    "navigator.geolocation.getCurrentPosition(function(){})");
        } else {
            // Use setTimeout to ensure any transient user gesture from the test runner is lost.
            mPermissionRule.runJavaScriptCodeInCurrentTab(
                    "setTimeout(function() { navigator.geolocation.getCurrentPosition(function(){})"
                            + " }, 0)");
        }
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @RequiresRestart("Reset geolocation settings.")
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testGeolocationWithoutGestureTriggersQuietUI() throws Exception {
        triggerGeolocationPermissionRequest(/* withGesture= */ false);

        // No gesture -> Quiet UI (MessageUI).
        mPermissionRule.waitForMessageShownState(true);

        onViewWaiting(withText("Location blocked")).check(matches(isDisplayed()));

        onViewWaiting(withText("OK")).perform(click());
        mPermissionRule.waitForGeolocationSettingForOrigin(
                /* usePrecise= */ true, ContentSetting.BLOCK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @RequiresRestart("Reset geolocation settings.")
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testGeolocationWithGestureTriggersLoudUI() throws Exception {
        triggerGeolocationPermissionRequest(/* withGesture= */ true);

        // With gesture -> Loud UI (Dialog).
        mPermissionRule.waitForDialogShownState(true);

        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW, mPermissionRule.getActivity());
        mPermissionRule.waitForDialogShownState(false);
        mPermissionRule.waitForGeolocationSettingForOrigin(
                /* usePrecise= */ true, ContentSetting.ALLOW, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testNotificationsNoGestureTriggersQuietUI() throws Exception {
        triggerNotificationPermissionRequest(/* withGesture= */ false);

        // No gesture -> Quiet UI(Icon with ClapperQuiet).
        waitForQuietIcon();
        mPermissionRule.waitForMessageShownState(false);

        clickClapperQuietIcon();
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID))
                .perform(click());
        pressBack();
        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testNotificationsWithGestureTriggersLoudUI() throws Exception {
        triggerNotificationPermissionRequest(/* withGesture= */ true);

        // With gesture -> Loud UI (MessageUI with ClapperQuiet).
        mPermissionRule.waitForMessageShownState(true);

        onViewWaiting(withText("Get notifications?")).check(matches(isDisplayed()));
        // Verify Allow. Clapper Loud uses a message UI.
        onViewWaiting(withId(PermissionTestRule.CLAPPER_LOUD_ALLOW_BUTTON_ID)).perform(click());
        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);
    }
}
