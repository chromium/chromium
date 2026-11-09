// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.os.Build;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.RootSpec;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewFinder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.WindowAndroid;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@EnableFeatures({PermissionsAndroidFeatureList.PERMISSIONS_ANDROID_CLAPPER_LOUD})
@Batch(Batch.PER_CLASS)
public class PermissionClapperLoudTest {

    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();
    public PermissionTestRule mPermissionRule = new PermissionTestRule(mActivityTestRule);

    @Rule
    public RuleChain mRuleChain = RuleChain.outerRule(mActivityTestRule).around(mPermissionRule);

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
        mActivityTestRule.startOnBlankPage();
        safelyClosePageInfo();
        clearPermissionDelegate();
        mPermissionRule.resetNotificationsSettingsForTest(/* enableQuietUi= */ false);
    }

    private void safelyClosePageInfo() {
        boolean wasShowing =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PageInfoController controller =
                                    PageInfoController.getLastPageInfoController();
                            if (controller != null && controller.isDialogShowing()) {
                                controller.dismiss();
                                return true;
                            }
                            return false;
                        });
        if (wasShowing) {
            mPermissionRule.waitForPageInfoClose();
        }
    }

    private void clickView(Matcher<View> matcher) {
        ViewFinder.waitForView(matcher, ViewElement.rootSpecOption(RootSpec.anyRoot())).click();
    }

    private void waitForView(Matcher<View> matcher) {
        ViewFinder.waitForView(matcher, ViewElement.rootSpecOption(RootSpec.anyRoot()));
    }

    private void closePageInfo() {
        ViewFinder.waitForView(
                        withId(R.id.page_info_url_wrapper),
                        ViewElement.rootSpecOption(RootSpec.anyRoot()))
                .performViewAction(pressBack());
        mPermissionRule.waitForPageInfoClose();
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
        WindowAndroid windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    windowAndroid.setAndroidPermissionDelegate(testDelegate);
                });
    }

    /** Clears the injected Android permission delegate. */
    private void clearPermissionDelegate() {
        WindowAndroid windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_ALLOW_BUTTON_ID));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);

        mPermissionRule.waitForMessageShownState(false);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_ALLOWED_ID);

        closePageInfo();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_ALLOW_BUTTON_ID));

        mPermissionRule.waitForMessageShownState(false);

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        closePageInfo();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_DONT_ALLOW_BUTTON_TEXT_ID));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);
        mPermissionRule.waitForMessageShownState(false);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

        mPermissionRule.waitForStatusIcon(PermissionTestRule.NOTIFICATIONS_NOT_ALLOWED_ID);

        mPermissionRule.openPageInfoFromStatusIcon();
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_BLOCKED_ID);

        closePageInfo();
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

        ViewFinder.waitForView(
                        withId(R.id.message_banner), ViewElement.rootSpecOption(RootSpec.anyRoot()))
                .performViewAction(swipeUp());

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);
        mPermissionRule.waitForMessageShownState(false);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT));

        mPermissionRule.waitForPageInfoOpen();

        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ALLOW, PAGE_URL);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

        closePageInfo();

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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT));

        mPermissionRule.waitForPageInfoOpen();

        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_SUBSCRIBE_BUTTON_TEXT_ID));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        waitForView(withText(R.string.page_info_permissions_title));

        clickView(withId(R.id.subpage_back_button));

        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();

        closePageInfo();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT));

        mPermissionRule.waitForPageInfoOpen();

        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID));
        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_DIALOG_BUTTON_TEXT));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();
        mPermissionRule.verifyNoPageInfoPermissionsRow(PermissionTestRule.NOTIFICATIONS_TITLE_ID);

        closePageInfo();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT));

        mPermissionRule.waitForPageInfoOpen();

        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_RESET_BUTTON_TEXT_ID));
        clickView(withText(PermissionTestRule.CLAPPER_PAGE_INFO_CANCEL_DIALOG_BUTTON_TEXT));

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.ASK, PAGE_URL);

        waitForView(withText(R.string.page_info_permissions_title));

        actionWatcher.assertExpected();
        actionWatcher = expectAction(PermissionTestRule.PromptAction.DENIED);
        clickView(withId(R.id.subpage_back_button));
        mPermissionRule.verifyPageInfoPermissionsRow(
                PermissionTestRule.NOTIFICATIONS_TITLE_ID,
                PermissionTestRule.PERMISSIONS_SUMMARY_BLOCKED_ID);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.assertExpected();

        closePageInfo();
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

        clickView(withId(PermissionTestRule.CLAPPER_LOUD_GEAR_BUTTON_ID));
        clickView(withText(PermissionTestRule.CLAPPER_LOUD_MANAGE_BUTTON_TEXT));

        mPermissionRule.waitForPageInfoOpen();

        closePageInfo();

        mPermissionRule.waitForPermissionSettingForOrigin(
                ContentSettingsType.NOTIFICATIONS, ContentSetting.BLOCK, PAGE_URL);

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        actionWatcher.pollInstrumentationThreadUntilSatisfied();
    }
}
