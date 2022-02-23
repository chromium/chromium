// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.ShadowBuildInfo;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.PermissionRequestMode;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionConstants;
import org.chromium.ui.permissions.PermissionPrefs;

import java.lang.ref.WeakReference;

/**
 * Robolectric unit tests for {@link NotificationPermissionController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class NotificationPermissionControllerTest {
    private static final long TEN_DAYS_IN_MILLIS = 10 * 24 * 3600 * 1000;

    @Before
    public void setUp() {
        ShadowBuildInfo.reset();
    }

    @After
    public void tearDown() {
        ShadowBuildInfo.reset();
    }

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<TestActivity>(TestActivity.class);

    @Test
    @Config(sdk = 29, manifest = Config.NONE, shadows = {ShadowBuildInfo.class})
    public void testNotificationPromptShownOnStartup_firstTime_hasPermissionAlready() {
        ShadowBuildInfo.setIsAtLeastT(true);
        mActivityScenarios.getScenario().onActivity(activity -> {
            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
            NotificationPermissionController notificationPermissionController =
                    new NotificationPermissionController(
                            new ActivityAndroidPermissionDelegate(
                                    new WeakReference<Activity>(activity)),
                            rationaleDelegate);

            // Has permission already. We shouldn't show anything.
            Shadows.shadowOf(activity).grantPermissions(
                    PermissionConstants.NOTIFICATION_PERMISSION);
            assertEquals(PermissionRequestMode.DO_NOT_REQUEST,
                    notificationPermissionController.shouldRequestPermission());
        });
    }

    @Test
    @Config(sdk = 29, manifest = Config.NONE, shadows = {ShadowBuildInfo.class})
    public void testNotificationPromptShownOnStartup_firstTime_noPermissionsYet() {
        ShadowBuildInfo.setIsAtLeastT(true);
        mActivityScenarios.getScenario().onActivity(activity -> {
            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
            NotificationPermissionController notificationPermissionController =
                    new NotificationPermissionController(
                            new ActivityAndroidPermissionDelegate(
                                    new WeakReference<Activity>(activity)),
                            rationaleDelegate);

            // First time ever. We should show OS prompt.
            Shadows.shadowOf(activity).denyPermissions(PermissionConstants.NOTIFICATION_PERMISSION);
            assertEquals(PermissionRequestMode.REQUEST_ANDROID_PERMISSION,
                    notificationPermissionController.shouldRequestPermission());
            assertEquals(0, PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp());
        });
    }

    @Test
    @Config(sdk = 29, manifest = Config.NONE, shadows = {ShadowBuildInfo.class})
    public void testNotificationPromptShownOnStartup_permissionShownOnce_tooSoonForSecondTime() {
        ShadowBuildInfo.setIsAtLeastT(true);
        mActivityScenarios.getScenario().onActivity(activity -> {
            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
            NotificationPermissionController notificationPermissionController =
                    new NotificationPermissionController(
                            new ActivityAndroidPermissionDelegate(
                                    new WeakReference<Activity>(activity)),
                            rationaleDelegate);

            // Show OS prompt shown for the first time.
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, false);
            notificationPermissionController.requestPermissionIfNeeded();
            assertNotEquals(0, PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp());

            // Try showing it for the second time before sufficient time has elapsed assuming user
            // dismissed the OS prompt by touching outside.
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, false);

            assertEquals(PermissionRequestMode.DO_NOT_REQUEST,
                    notificationPermissionController.shouldRequestPermission());

            // Try showing it for the second time before sufficient time has elapsed assuming user
            // dismissed the OS prompt by touching outside.
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, true);

            assertEquals(PermissionRequestMode.DO_NOT_REQUEST,
                    notificationPermissionController.shouldRequestPermission());
        });
    }

    @Test
    @Config(sdk = 29, manifest = Config.NONE, shadows = {ShadowBuildInfo.class})
    public void testNotificationPromptShownOnStartup_permissionShownOnce_showAfterTenDays() {
        ShadowBuildInfo.setIsAtLeastT(true);
        mActivityScenarios.getScenario().onActivity(activity -> {
            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
            NotificationPermissionController notificationPermissionController =
                    new NotificationPermissionController(
                            new ActivityAndroidPermissionDelegate(new WeakReference<>(activity)),
                            rationaleDelegate);

            // Show OS prompt shown for the first time.
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, false);
            notificationPermissionController.requestPermissionIfNeeded();

            // Try showing the second time after 10 days.
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putLong("AndroidPermissionRequestTimestamp::"
                                    + PermissionConstants.NOTIFICATION_PERMISSION,
                            System.currentTimeMillis() - TEN_DAYS_IN_MILLIS)
                    .commit();
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, true);

            assertEquals(PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE,
                    notificationPermissionController.shouldRequestPermission());
        });
    }

    @Test
    @Config(sdk = 29, manifest = Config.NONE, shadows = {ShadowBuildInfo.class})
    public void testNotificationPromptShownOnStartup_permissionShownOnce_noThirdTime() {
        ShadowBuildInfo.setIsAtLeastT(true);
        mActivityScenarios.getScenario().onActivity(activity -> {
            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
            NotificationPermissionController notificationPermissionController =
                    new NotificationPermissionController(
                            new ActivityAndroidPermissionDelegate(new WeakReference<>(activity)),
                            rationaleDelegate);

            // Show OS prompt shown for the first time.
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, false);
            notificationPermissionController.requestPermissionIfNeeded();

            // Show for the second time after 10 days with rationale.
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putLong("AndroidPermissionRequestTimestamp::"
                                    + PermissionConstants.NOTIFICATION_PERMISSION,
                            System.currentTimeMillis() - TEN_DAYS_IN_MILLIS)
                    .commit();
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, true);
            Integer rationaleDialogAction = DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
            rationaleDelegate.setDialogAction(rationaleDialogAction);
            notificationPermissionController.requestPermissionIfNeeded();

            // Try for a third time after 10 days. We shouldn't show.
            SharedPreferencesManager.getInstance().writeLong(
                    ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                    System.currentTimeMillis() - TEN_DAYS_IN_MILLIS);
            Shadows.shadowOf(activity.getPackageManager())
                    .setShouldShowRequestPermissionRationale(
                            PermissionConstants.NOTIFICATION_PERMISSION, true);

            assertEquals(PermissionRequestMode.DO_NOT_REQUEST,
                    notificationPermissionController.shouldRequestPermission());
        });
    }
}
