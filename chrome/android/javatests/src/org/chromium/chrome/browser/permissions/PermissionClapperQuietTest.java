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
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.common.ContentSwitches;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DisableFeatures({ChromeFeatureList.SETTINGS_MULTI_COLUMN})
@EnableFeatures({
    PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_QUIET,
    PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_LOUD
})
@Batch(Batch.PER_CLASS)
public class PermissionClapperQuietTest {

    @Rule public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String PAGE_URL = "/content/test/data/android/permission_navigation.html";

    private HistogramWatcher expectActionTimes(
            @PermissionTestRule.PromptAction int action, int times) {
        return HistogramWatcher.newBuilder()
                .expectIntRecordTimes(
                        "Permissions.Prompt.Notifications.LocationBarLeftClapperQuietIcon.Action",
                        action,
                        times)
                .expectIntRecordTimes(
                        "Permissions.Action.WithDisposition.LocationBarLeftClapperQuietIcon",
                        action,
                        times)
                .build();
    }

    private HistogramWatcher expectAction(@PermissionTestRule.PromptAction int action) {
        return expectActionTimes(action, 1);
    }

    private HistogramWatcher expectNoClapperQuietRecords() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords(
                        "Permissions.Prompt.Notifications.LocationBarLeftClapperQuietIcon.Action")
                .expectNoRecords(
                        "Permissions.Action.WithDisposition.LocationBarLeftClapperQuietIcon")
                .build();
    }

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
        mPermissionRule.resetNotificationsSettingsForTest(/* enableQuietUi= */ true);
    }

    private void waitForQuietIcon() {

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);
    }

    private void clickClapperQuietIcon() {

        mPermissionRule.openPageInfoFromStatusIcon();
    }

    private void waitForQuietIconGone() {

        mPermissionRule.waitForStatusIconGone(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);
    }

    private void waitForSecurityIcon() {
        // Wait for the security icon (or any default status icon) to be stable.
        // This prevents race conditions where late security state updates (e.g. from page load)
        // trigger a StatusMediator reset, wiping out the subsequently shown permission icon.
        // We poll until the View hierarchy shows an icon that is NOT the permission icon.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        waitForQuietIconGone();
                        onView(withId(R.id.location_bar_status_icon)).check(matches(isDisplayed()));
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    private void setupPageAndTriggerNotificationPermissionRequest(String url) throws Exception {
        if (url.startsWith("http")) {
            mPermissionRule.loadUrl(url);
        } else {
            mPermissionRule.setUpUrl(url);
        }

        // Wait for the security state to stabilize before triggering the permission.
        waitForSecurityIcon();

        // Inject a click handler to satisfy the User Gesture (Transient Activation) requirement.
        // Notification.requestPermission() must be called from a user interaction.
        // PermissionTestRule.runJavaScriptCodeInCurrentTabWithGesture() simulates a tap,
        // but permission_navigation.html is a generic page without an onclick listener.
        // This injection ensures the tap executes the function stored in 'window.functionToRun'.
        mPermissionRule.runJavaScriptCodeInCurrentTab(
                "window.onclick = function() { if (window.functionToRun) {"
                        + " eval(window.functionToRun); } };");

        // Trigger notification permission with a gesture.
        mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                "Notification.requestPermission()");
    }

    private void triggerQuietClapper() throws Exception {
        setupPageAndTriggerNotificationPermissionRequest(PAGE_URL);
        mPermissionRule.waitForOmniboxPermissionState(ContentSettingsType.NOTIFICATIONS);
    }

    private void triggerQuietIconTimeout() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarCoordinator locationBarCoordinator =
                            (LocationBarCoordinator)
                                    mPermissionRule
                                            .getActivity()
                                            .getToolbarManager()
                                            .getLocationBar();
                    locationBarCoordinator
                            .getStatusCoordinator()
                            .getMediatorForTesting()
                            .getPermissionStatusHandler()
                            .triggerIconTimeoutForTesting();
                });
    }

    private void checkQuietIconTimerState(boolean expectedIsRunning) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarCoordinator locationBarCoordinator =
                            (LocationBarCoordinator)
                                    mPermissionRule
                                            .getActivity()
                                            .getToolbarManager()
                                            .getLocationBar();
                    boolean isRunning =
                            locationBarCoordinator
                                    .getStatusCoordinator()
                                    .getMediatorForTesting()
                                    .getPermissionStatusHandler()
                                    .isIconTimeoutRunningForTesting();
                    if (expectedIsRunning) {

                        Assert.assertTrue(
                                "Icon dismissal timer should still be running", isRunning);
                    } else {
                        Assert.assertFalse(
                                "Icon dismissal timer should have been cancelled", isRunning);
                    }
                });
    }

    private CallbackHelper setDismissedCallback() {
        CallbackHelper onDismissedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarCoordinator locationBarCoordinator =
                            (LocationBarCoordinator)
                                    mPermissionRule
                                            .getActivity()
                                            .getToolbarManager()
                                            .getLocationBar();
                    locationBarCoordinator
                            .getStatusCoordinator()
                            .getMediatorForTesting()
                            .getPermissionStatusHandler()
                            .setOnIconDismissedCallbackForTesting(
                                    onDismissedCallback::notifyCalled);
                });
        return onDismissedCallback;
    }

    private CallbackHelper setTabSwitcherTransitionFinishedCallback() {
        CallbackHelper onTabSwitchCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarCoordinator locationBarCoordinator =
                            (LocationBarCoordinator)
                                    mPermissionRule
                                            .getActivity()
                                            .getToolbarManager()
                                            .getLocationBar();
                    locationBarCoordinator
                            .getStatusCoordinator()
                            .getMediatorForTesting()
                            .getPermissionStatusHandler()
                            .setTabSwitchCallbackForTesting(onTabSwitchCallback::notifyCalled);
                });
        return onTabSwitchCallback;
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperIconShown() throws Exception {
        triggerQuietClapper();
        waitForQuietIcon();

        // Assert no quiet message is shown.
        // This verifies that the "Message" quiet UI was suppressed.
        mPermissionRule.waitForMessageShownState(false);

        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @DisableFeatures({PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_QUIET})
    public void testStandardQuietUiShowsMessage() throws Exception {

        mPermissionRule.setupPageAndTriggerNotificationPermissionRequest(PAGE_URL);

        // With Clapper disabled, the standard Quiet UI (Message) should be shown.
        mPermissionRule.waitForMessageShownState(true);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperIgnoreIcon() throws Exception {
        HistogramWatcher histogramWatcher =
                expectActionTimes(PermissionTestRule.PromptAction.IGNORED, /* times= */ 2);
        CallbackHelper onDismissedCallback = setDismissedCallback();

        triggerQuietClapper();
        waitForQuietIcon();
        triggerQuietIconTimeout();
        onDismissedCallback.waitForCallback(0);

        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        triggerQuietClapper();
        waitForQuietIcon();
        triggerQuietIconTimeout();
        onDismissedCallback.waitForCallback(0);

        // For quiet prompts, we block the page after 2 ignores.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperSubscribe() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.GRANTED);
        triggerQuietClapper();
        waitForQuietIcon();

        clickClapperQuietIcon();

        // Clicking the icon stops the auto-ignore timer
        checkQuietIconTimerState(/* expectedIsRunning= */ false);

        // The settings should not have changed yet.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
        // Wait for and click the "Subscribe" button.
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID))
                .perform(click());

        // Verify permission is granted.
        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);

        histogramWatcher.assertExpected();

        pressBack();
        mPermissionRule.waitForPageInfoClose();
        waitForQuietIconGone();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperReset() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.DISMISSED);
        triggerQuietClapper();
        waitForQuietIcon();

        clickClapperQuietIcon();

        // Wait for and click the "Reset permissions" button.
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID))
                .perform(click());

        // Confirm the reset in the dialog.
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_DIALOG_BUTTON_TEXT))
                .perform(click());

        // Because of the embargo logic for quiet permission prompts the permission gets blocked
        // after the first dismissal for this origin.
        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperDenyViaSystemBack() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);
        triggerQuietClapper();
        waitForQuietIcon();

        clickClapperQuietIcon();
        mPermissionRule.waitForPageInfoOpen();

        // Close Page Info via system Back press.
        pressBack();
        mPermissionRule.waitForPageInfoClose();
        waitForQuietIconGone();

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperDenyViaBackButton() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);
        triggerQuietClapper();
        waitForQuietIcon();

        clickClapperQuietIcon();
        mPermissionRule.waitForPageInfoOpen();

        // Dismiss Page Info via the subpage back button.
        // This should lead us to the "main" PageInfo component.
        onView(withId(R.id.subpage_back_button)).perform(click());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.assertExpected();

        // Close Page Info to ensure clean teardown.
        pressBack();
        mPermissionRule.waitForPageInfoClose();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperNavigation() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);
        triggerQuietClapper();
        waitForQuietIcon();

        // Navigate away.
        mPermissionRule.loadUrl(mPermissionRule.getURL("/content/test/data/android/simple.html"));

        waitForQuietIconGone();
        // Navigation acts as an ignore. For clapper quiet, 1 ignore does not block the origin.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperReload() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);
        triggerQuietClapper();
        waitForQuietIcon();

        // Reload the page.
        mPermissionRule.loadUrl(mPermissionRule.getURL(PAGE_URL));

        waitForQuietIconGone();
        // Reload acts as an ignore. For clapper quiet, 1 ignore does not block the origin.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @RequiresRestart("Tab switching tests are flaky in batch")
    public void testQuietClapperTabSwitcherButton() throws Exception {
        HistogramWatcher histogramWatcher = expectNoClapperQuietRecords();
        CallbackHelper onTabSwitchCallback = setTabSwitcherTransitionFinishedCallback();

        triggerQuietClapper();
        waitForQuietIcon();

        // Enter Tab Switcher via UI.

        TabUiTestHelper.enterTabSwitcher(
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity());

        onTabSwitchCallback.waitForCallback(0);

        histogramWatcher.assertExpected();

        ChromeTabUtils.switchTabInCurrentTabModel(
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity(),
                0);

        // The icon should reappear when switching back.
        waitForQuietIcon();
        histogramWatcher.assertExpected();

        histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);
        CallbackHelper onDismissedCallback = setDismissedCallback();
        triggerQuietIconTimeout();
        onDismissedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @RequiresRestart("Tab switching tests are flaky in batch")
    public void testQuietClapperNewTabButton() throws Exception {
        HistogramWatcher histogramWatcher = expectNoClapperQuietRecords();
        CallbackHelper onTabSwitchCallback = setTabSwitcherTransitionFinishedCallback();

        triggerQuietClapper();
        waitForQuietIcon();

        // Enter Tab Switcher via UI.
        TabUiTestHelper.enterTabSwitcher(
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity());
        onTabSwitchCallback.waitForCallback(0);

        TabUiTestHelper.leaveTabSwitcher(
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity());

        // The icon should reappear when switching back.
        waitForQuietIcon();
        histogramWatcher.assertExpected();

        histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);
        CallbackHelper onDismissedCallback = setDismissedCallback();
        triggerQuietIconTimeout();
        onDismissedCallback.waitForCallback(0);
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    @DisableFeatures({PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_LOUD})
    public void testLoudPromptDenyDoesNotTriggerQuietDismiss() throws Exception {
        // This test ensures that when a standard (loud) permission prompt is denied, the subsequent
        // "Blocked" icon (which shares UI components with the Quiet icon) does NOT trigger the
        // Quiet UI specific logic (e.g., logging "Ignored" metrics after a timeout).
        // The icon should still disappear automatically, but without side effects.
        HistogramWatcher histogramWatcher = expectNoClapperQuietRecords();

        CallbackHelper onDismissedCallback = setDismissedCallback();
        // Force Loud UI by disabling the Quiet UI pref.
        mPermissionRule.resetNotificationsSettingsForTest(/* enableQuietUi= */ false);

        mPermissionRule.setupPageAndTriggerNotificationPermissionRequest(PAGE_URL);

        // Verify Loud Prompt (Dialog) is shown.
        mPermissionRule.waitForDialogShownState(true);

        // Deny the prompt.
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mPermissionRule.getActivity());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        // Wait for the blocked icon (which gets reused by the quiet icon logic).
        waitForQuietIcon();

        triggerQuietIconTimeout();

        // The icon should disappear (timer logic runs).
        // We verify the UI behavior (icon disappearance) and that the callback fired.
        onDismissedCallback.waitForCallback(0);
        waitForQuietIconGone();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testInterferenceIgnoresRequest() throws Exception {
        // This test simulates "Interference" (user focusing the Omnibox) while the Quiet Icon is
        // shown. This action should reset the UI and implicitly Ignore the permission request.
        // We verify that the native-side request state is correctly updated to IGNORED.
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        CallbackHelper onDismissedCallback = setDismissedCallback();
        triggerQuietClapper();

        waitForQuietIcon();

        // Simulate an interference event: Focus the Omnibox.
        onView(withId(R.id.url_bar)).perform(click());

        onDismissedCallback.waitForCallback(0);

        histogramWatcher.assertExpected();
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperCookiesControlsInteraction() throws Exception {
        // This test simulates a Cookie Controls icon event (e.g., highlighting) while the Quiet
        // Icon is shown. This high-priority icon update should preempt the quiet icon, causing it
        // to be removed and the request to be Ignored.
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        triggerQuietClapper();
        waitForQuietIcon();

        // Trigger the Cookie Controls icon animation via the StatusMediator.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocationBarCoordinator locationBarCoordinator =
                            (LocationBarCoordinator)
                                    mPermissionRule
                                            .getActivity()
                                            .getToolbarManager()
                                            .getLocationBar();
                    locationBarCoordinator
                            .getStatusCoordinator()
                            .getMediatorForTesting()
                            .onHighlightCookieControl(true);
                });

        // The quiet icon should be replaced/removed.
        waitForQuietIconGone();

        histogramWatcher.assertExpected();
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperTabDestroyed() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                (org.chromium.chrome.browser.ChromeTabbedActivity) mPermissionRule.getActivity(),
                mPermissionRule.getURL(PAGE_URL),
                /* incognito= */ false);

        triggerQuietClapper();
        waitForQuietIcon();

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mPermissionRule.getActivity());

        waitForQuietIconGone();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperPreemption() throws Exception {
        // Test that a high-priority request preempts the quiet UI and removes the icon.
        // We expect the quiet request to be ignored when preempted.
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        triggerQuietClapper();
        waitForQuietIcon();

        // Trigger a high-priority permission request (Microphone) to preempt the quiet one.
        mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                "navigator.mediaDevices.getUserMedia({audio: true})");

        // The Microphone prompt (Loud UI) should be shown.
        // Waiting for this first ensures the new request was processed by PermissionRequestManager.
        mPermissionRule.waitForDialogShownState(true);

        // The quiet icon should be removed immediately.
        waitForQuietIconGone();

        histogramWatcher.assertExpected();
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperPreemptionWhilePageInfoOpenSubscribe() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        triggerQuietClapper();
        waitForQuietIcon();
        clickClapperQuietIcon();
        mPermissionRule.waitForPageInfoOpen();

        // Trigger a high-priority permission request (Microphone) to preempt the quiet one.
        mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                "navigator.mediaDevices.getUserMedia({audio: true})");
        // The Microphone prompt (Loud UI) should be shown.
        mPermissionRule.waitForDialogShownState(true);

        // Page Info should still be open. Attempt to Subscribe.
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID))
                .perform(click());

        // Verify that the permission was NOT granted (because the request was preempted).
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        // Verify no crash.
        histogramWatcher.assertExpected();

        // Cleanup
        pressBack();
        mPermissionRule.waitForPageInfoClose();
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mPermissionRule.getActivity());

        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperPreemptionWhilePageInfoOpenReset() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        triggerQuietClapper();
        waitForQuietIcon();
        clickClapperQuietIcon();
        mPermissionRule.waitForPageInfoOpen();

        // Trigger preemption.
        mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                "navigator.mediaDevices.getUserMedia({audio: true})");
        mPermissionRule.waitForDialogShownState(true);

        // Attempt to Reset.
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID))
                .perform(click());
        onViewWaiting(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_DIALOG_BUTTON_TEXT))
                .perform(click());

        // Verify that the permission was NOT blocked (default for reset) or changed.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.assertExpected();

        // Cleanup
        pressBack();
        mPermissionRule.waitForPageInfoClose();
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mPermissionRule.getActivity());

        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }

    @Test
    @MediumTest
    @Feature({"Permissions"})
    public void testQuietClapperPreemptionWhilePageInfoOpenDismiss() throws Exception {
        HistogramWatcher histogramWatcher = expectAction(PermissionTestRule.PromptAction.IGNORED);

        triggerQuietClapper();
        waitForQuietIcon();
        clickClapperQuietIcon();
        mPermissionRule.waitForPageInfoOpen();

        // Trigger preemption.
        mPermissionRule.runJavaScriptCodeInCurrentTabWithGesture(
                "navigator.mediaDevices.getUserMedia({audio: true})");
        mPermissionRule.waitForDialogShownState(true);

        // Dismiss Page Info via system Back press.
        pressBack();
        mPermissionRule.waitForPageInfoClose();

        histogramWatcher.assertExpected();

        // Cleanup
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mPermissionRule.getActivity());

        // Verify permission state remains ASK.
        mPermissionRule.checkPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);
    }
}
