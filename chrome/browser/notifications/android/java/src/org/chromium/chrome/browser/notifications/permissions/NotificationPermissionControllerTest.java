// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.UserManager;
import android.text.format.DateUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowUserManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationPermissionState;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.PermissionRequestMode;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController.RationaleDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionPrefs;

import java.lang.ref.WeakReference;

/** Robolectric unit tests for {@link NotificationPermissionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.TIRAMISU, manifest = Config.NONE)
public class NotificationPermissionControllerTest {
    private static final int DEMO_USER_ID = 2;
    private ShadowUserManager mShadowUserManager;

    @Rule public FakeTimeTestRule mFakeTimeRule = new FakeTimeTestRule();

    @Before
    public void setUp() {
        mShadowUserManager =
                shadowOf(
                        ApplicationProvider.getApplicationContext()
                                .getSystemService(UserManager.class));
        mShadowUserManager.addUser(DEMO_USER_ID, "demo_user", ShadowUserManager.FLAG_DEMO);

        setupFeatureParams(false, null, null);
    }

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<TestActivity>(TestActivity.class);

    private NotificationPermissionController createNotificationPermissionController(
            Activity activity) {
        return createNotificationPermissionController(new TestRationaleDelegate(), activity);
    }

    private NotificationPermissionController createNotificationPermissionController(
            RationaleDelegate rationaleDelegate, Activity activity) {
        return createNotificationPermissionController(
                rationaleDelegate,
                new TestAndroidPermissionDelegate(new WeakReference<>(activity)));
    }

    private NotificationPermissionController createNotificationPermissionController(
            RationaleDelegate rationaleDelegate,
            TestAndroidPermissionDelegate androidPermissionDelegate) {
        return new NotificationPermissionController(
                androidPermissionDelegate, () -> rationaleDelegate);
    }

    private void verifyStateHistogramWasRecorded(
            @NotificationPermissionState int state, int expectedTimes) {
        assertEquals(
                expectedTimes,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.StartupState", state));
    }

    private void verifyPermissionRequestCountHistogram(int expectedCount) {
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.StartupRequestCount", expectedCount));
    }

    private void verifyOSPermissionResultHistogram(boolean hasPermission) {
        int histogramValue = hasPermission ? 1 : 0;

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Permission.OSPromptResult", histogramValue));
    }

    private void grantNotificationPermission(Activity activity) {
        shadowOf(activity).grantPermissions(Manifest.permission.POST_NOTIFICATIONS);
    }

    private void invokeOSPermissionCallback(
            TestAndroidPermissionDelegate delegate, boolean permissionGranted) {
        int resultValue =
                permissionGranted
                        ? PackageManager.PERMISSION_GRANTED
                        : PackageManager.PERMISSION_DENIED;

        delegate.invokePermissionResultCallbackForLastRequest(
                new String[] {Manifest.permission.POST_NOTIFICATIONS}, new int[] {resultValue});
    }

    private void setShouldShowRequestPermissionRationale(
            Activity activity, boolean shouldShowRequestPermissionRationale) {
        shadowOf(activity.getPackageManager())
                .setShouldShowRequestPermissionRationale(
                        Manifest.permission.POST_NOTIFICATIONS,
                        shouldShowRequestPermissionRationale);
    }

    private void setupFeatureParams(
            Boolean alwaysShowRationale,
            Integer permissionRequestMaxCount,
            Integer requestIntervalDays) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        if (alwaysShowRationale != null) {
            testValues.addFieldTrialParamOverride(
                    ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                    NotificationPermissionController
                            .FIELD_TRIAL_ALWAYS_SHOW_RATIONALE_BEFORE_REQUESTING_PERMISSION,
                    Boolean.toString(alwaysShowRationale));
        }
        if (permissionRequestMaxCount != null) {
            testValues.addFieldTrialParamOverride(
                    ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                    NotificationPermissionController.FIELD_TRIAL_PERMISSION_REQUEST_MAX_COUNT,
                    Integer.toString(permissionRequestMaxCount));
        }
        if (requestIntervalDays != null) {
            testValues.addFieldTrialParamOverride(
                    ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                    NotificationPermissionController.FIELD_TRIAL_PERMISSION_REQUEST_INTERVAL_DAYS,
                    Integer.toString(requestIntervalDays));
        }
        FeatureList.setTestValues(testValues);
    }

    private static class TestAndroidPermissionDelegate extends ActivityAndroidPermissionDelegate {
        private int mLastRequestCode;

        public TestAndroidPermissionDelegate(WeakReference<Activity> activity) {
            super(activity);
        }

        @Override
        protected boolean requestPermissionsFromRequester(String[] permissions, int requestCode) {
            mLastRequestCode = requestCode;
            return super.requestPermissionsFromRequester(permissions, requestCode);
        }

        void invokePermissionResultCallbackForLastRequest(
                String[] permissions, int[] grantResults) {
            handlePermissionResult(mLastRequestCode, permissions, grantResults);
        }
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    public void testNotificationPrompt_nothingHappensWhenNotTargetingT() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            notificationPermissionController.requestPermissionIfNeeded();

                            long permissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // We shouldn't have requested for permission or shown the rationale.
                            assertEquals(0, rationaleDelegate.getCallCount());
                            assertEquals(0, permissionRequestTimestamp);
                        });
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testNotificationPrompt_nothingHappensWhenNotRunningOnT() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            notificationPermissionController.requestPermissionIfNeeded();

                            long permissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // We shouldn't have requested for permission or shown the rationale.
                            assertEquals(0, rationaleDelegate.getCallCount());
                            assertEquals(0, permissionRequestTimestamp);
                        });
    }

    @Test
    public void testNotificationPrompt_nothingHappensInDemoMode() {
        mShadowUserManager.switchUser(DEMO_USER_ID);

        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            notificationPermissionController.requestPermissionIfNeeded();

                            long permissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // We shouldn't have requested for permission or shown the rationale.
                            assertEquals(0, rationaleDelegate.getCallCount());
                            assertEquals(0, permissionRequestTimestamp);
                        });
    }

    @Test
    public void testNotificationPrompt_alreadyHasPermission() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            // First attempt, should show OS prompt.
                            notificationPermissionController.requestPermissionIfNeeded();

                            // Reject the OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);

                            // 10 days have passed since last request.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            // If the user hits "Deny" on the permission dialog then we have one
                            // more chance to ask again, so Android tells us to show a rationale
                            // first.
                            setShouldShowRequestPermissionRationale(activity, true);

                            // Set the rationale dialog to be approved, this should trigger a new OS
                            // prompt.
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

                            // Second attempt, should show rationale, then OS prompt.
                            notificationPermissionController.requestPermissionIfNeeded();
                            // Accept the OS prompt.
                            grantNotificationPermission(activity);
                            invokeOSPermissionCallback(permissionDelegate, true);

                            long secondPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // 15 days have passed since the second request.
                            mFakeTimeRule.advanceMillis(15 * DateUtils.DAY_IN_MILLIS);

                            // Call method a couple of times, nothing should happen.
                            notificationPermissionController.requestPermissionIfNeeded();
                            notificationPermissionController.requestPermissionIfNeeded();

                            // We should have shown the rationale only once.
                            assertEquals(1, rationaleDelegate.getCallCount());

                            // The permission request shouldn't show up on the third and fourth
                            // calls to requestPermissionIfNeeded() as we already have permission.
                            assertNotEquals(0, secondPermissionRequestTimestamp);
                            assertEquals(
                                    secondPermissionRequestTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());

                            // We should record a histogram value indicating we've asked for
                            // permission 2 times, showing the rationale and the OS prompt in one
                            // call to requestPermissionIfNeeded counts as one attempt.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 2);
                            // The last 2 calls to requestPermissionIfNeeded() should have recorded
                            // the state as "Already has permission, rationale was shown before".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.ALLOWED, /* expectedTimes= */ 2);
                        });
    }

    @Test
    public void testNotificationPromptShownOnStartup_noPermissionsYet_shouldShowOSPrompt() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(activity);

                            // First time ever. We should show OS prompt.
                            shadowOf(activity)
                                    .denyPermissions(Manifest.permission.POST_NOTIFICATIONS);

                            notificationPermissionController.requestPermissionIfNeeded();

                            // We should have showed the OS prompt.
                            assertNotEquals(
                                    0,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());
                            // The recorded state is "Never asked" because we record at the
                            // beginning.
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_NEVER_ASKED,
                                    /* expectedTimes= */ 1);
                            // We should have recorded a metric indicating we requested for
                            // permission once.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 1);
                        });
    }

    @Test
    public void testNotificationPromptShownOnStartup_alwaysShowRationale() {
        setupFeatureParams(true, 3, null);

        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<Activity>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show rationale for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);
                            assertEquals(
                                    PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE,
                                    notificationPermissionController.shouldRequestPermission());

                            Integer rationaleDialogAction =
                                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
                            rationaleDelegate.setDialogAction(rationaleDialogAction);
                            notificationPermissionController.requestPermissionIfNeeded();
                            assertEquals(
                                    0,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());
                            assertNotEquals(
                                    0,
                                    ContextUtils.getAppSharedPreferences()
                                            .getLong(
                                                    ChromePreferenceKeys
                                                            .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                                    0));

                            // Try showing it for the second time before sufficient time has elapsed
                            // assuming user dismissed the OS prompt by touching outside.
                            setShouldShowRequestPermissionRationale(activity, false);
                            assertEquals(
                                    PermissionRequestMode.DO_NOT_REQUEST,
                                    notificationPermissionController.shouldRequestPermission());

                            // Try showing the second time after 10 days. This time click positive
                            // button on rationale and negative on OS prompt.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);
                            assertEquals(
                                    PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE,
                                    notificationPermissionController.shouldRequestPermission());
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            notificationPermissionController.requestPermissionIfNeeded();

                            invokeOSPermissionCallback(permissionDelegate, false);
                            assertNotEquals(
                                    0,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());

                            // Cannot request permission right away.
                            assertEquals(
                                    PermissionRequestMode.DO_NOT_REQUEST,
                                    notificationPermissionController.shouldRequestPermission());

                            // After 10 days. Request permission again. This time again click
                            // positive button on rationale and negative on OS prompt.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);
                            setShouldShowRequestPermissionRationale(activity, true);
                            assertEquals(
                                    PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE,
                                    notificationPermissionController.shouldRequestPermission());
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            notificationPermissionController.requestPermissionIfNeeded();
                            invokeOSPermissionCallback(permissionDelegate, false);

                            // After 10 days. Request permission again. Can't request this time.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);
                            setShouldShowRequestPermissionRationale(activity, false);
                            assertEquals(
                                    PermissionRequestMode.DO_NOT_REQUEST,
                                    notificationPermissionController.shouldRequestPermission());
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndAccept() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);
                            notificationPermissionController.requestPermissionIfNeeded();

                            // Accept OS prompt.
                            grantNotificationPermission(activity);
                            invokeOSPermissionCallback(permissionDelegate, true);

                            long firstPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // 1 day later.
                            mFakeTimeRule.advanceMillis(1 * DateUtils.DAY_IN_MILLIS);

                            // Subsequent calls to this method won't do anything, we already got
                            // permission.
                            notificationPermissionController.requestPermissionIfNeeded();

                            // OS Prompt should have been shown only once.
                            assertNotEquals(0, firstPermissionRequestTimestamp);
                            assertEquals(
                                    firstPermissionRequestTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());

                            // The first call to requestPermissionIfNeeded() should record a metric
                            // indicating we have never asked before.
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_NEVER_ASKED,
                                    /* expectedTimes= */ 1);
                            // The second call to requestPermissionIfNeeded() should record a metric
                            // indicating the permission was granted without having to show the
                            // rationale.
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.ALLOWED, /* expectedTimes= */ 1);
                            // We should have recorded a metric indicating the OS prompt was
                            // accepted once.
                            verifyOSPermissionResultHistogram(true);
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndDismiss_tooSoonForSecondTime() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();

                            // Dismiss the OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            long firstOSPromptTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // 1 day later.
                            mFakeTimeRule.advanceMillis(1 * DateUtils.DAY_IN_MILLIS);

                            // Try showing it for the second time before sufficient time has elapsed
                            // assuming user dismissed the OS prompt by touching outside.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();

                            // OS Prompt should only be shown once.
                            assertEquals(
                                    firstOSPromptTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());
                            // The second call to requestPermissionIfNeeded() should have recorded
                            // the state as "Asked once, dismissed, waiting to show again".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_ONCE,
                                    /* expectedTimes= */ 1);
                            // We should have recorded a metric indicating the OS prompt was
                            // rejected once.
                            verifyOSPermissionResultHistogram(false);
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndReject_tooSoonForSecondTime() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();

                            // Dismiss the OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            long firstOSPromptTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // 1 day later.
                            mFakeTimeRule.advanceMillis(1 * DateUtils.DAY_IN_MILLIS);

                            // Try showing it for the second time before sufficient time has elapsed
                            // assuming user rejected the OS prompt by tapping "Deny".
                            setShouldShowRequestPermissionRationale(activity, true);

                            notificationPermissionController.requestPermissionIfNeeded();

                            // OS Prompt should only be shown once.
                            assertEquals(
                                    firstOSPromptTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());
                            // The second call to requestPermissionIfNeeded() should have recorded
                            // the state as "Asked once, dismissed, waiting to show again".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_ONCE,
                                    /* expectedTimes= */ 1);
                            // We should have recorded a metric indicating the OS prompt was
                            // rejected once.
                            verifyOSPermissionResultHistogram(false);
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndDismiss_showAgainAfterTimePasses() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt shown for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();
                            long firstPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Dismiss OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            // If the permission dialog is dismissed then it doesn't count towards
                            // the request limit, so Android tells us we shouldn't show the
                            // rationale yet.
                            setShouldShowRequestPermissionRationale(activity, false);

                            // 10 days have passed.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            notificationPermissionController.requestPermissionIfNeeded();
                            long secondPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // The OS permission prompt should have appeared again after waiting.
                            assertNotEquals(0, firstPermissionRequestTimestamp);
                            assertNotEquals(0, secondPermissionRequestTimestamp);
                            assertTrue(
                                    secondPermissionRequestTimestamp
                                            > firstPermissionRequestTimestamp);

                            // We should record a histogram value indicating we've asked for
                            // permission 2 times.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 2);
                            // The last call to requestPermissionIfNeeded() should have recorded the
                            // state as "Asked once, dismissed, showing again".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_ONCE,
                                    /* expectedTimes= */ 1);
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndReject_showRationaleAfterTimePasses() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();
                            long firstPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Reject OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            // If the user hits "Deny" on the permission dialog then we have one
                            // more chance to ask again, so Android tells us to show a rationale
                            // first.
                            setShouldShowRequestPermissionRationale(activity, true);

                            // 10 days have passed.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            // Set the rationale dialog to be dismissed.
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

                            notificationPermissionController.requestPermissionIfNeeded();

                            long rationaleTimestamp =
                                    ContextUtils.getAppSharedPreferences()
                                            .getLong(
                                                    ChromePreferenceKeys
                                                            .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                                    0);

                            // We should have shown the rationale.
                            assertNotEquals(0, rationaleTimestamp);
                            assertEquals(1, rationaleDelegate.getCallCount());
                            // We shouldn't show the OS prompt a second time.
                            assertEquals(
                                    firstPermissionRequestTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());

                            // We should record a histogram value indicating we've asked for
                            // permission 2 times, showing the rationale counts as an attempt.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 2);
                            // The last call to requestPermissionIfNeeded() should have recorded the
                            // state as "Asked once, denied, showing rationale".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_ONCE,
                                    /* expectedTimes= */ 1);
                        });
    }

    @Test
    public void
            testNotificationPrompt_showOSPromptAndReject_showRationaleAndAccept_approveSecondOSPrompt() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt shown for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            notificationPermissionController.requestPermissionIfNeeded();
                            long firstPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Reject OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            // If the user hits "Deny" on the permission dialog then we have one
                            // more chance to ask again, so Android tells us to show a rationale
                            // first.
                            setShouldShowRequestPermissionRationale(activity, true);

                            // 10 days have passed since last request.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            // Set the rationale dialog to be approved.
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

                            notificationPermissionController.requestPermissionIfNeeded();
                            long rationaleTimestamp =
                                    ContextUtils.getAppSharedPreferences()
                                            .getLong(
                                                    ChromePreferenceKeys
                                                            .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                                    0);
                            long secondPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // We should have shown the rationale.
                            assertNotEquals(0, rationaleTimestamp);
                            assertEquals(1, rationaleDelegate.getCallCount());

                            // As the rationale was accepted we should have also shown the
                            // permission prompt again.
                            assertNotEquals(0, firstPermissionRequestTimestamp);
                            assertNotEquals(0, secondPermissionRequestTimestamp);
                            assertTrue(
                                    secondPermissionRequestTimestamp
                                            > firstPermissionRequestTimestamp);

                            // We should record a histogram value indicating we've asked for
                            // permission 2 times, showing the rationale and the OS prompt in one
                            // call to requestPermissionIfNeeded() counts as one attempt.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 2);
                            // The last call to requestPermissionIfNeeded() should have recorded the
                            // state as "Asked once, denied, showing rationale", we record this
                            // state at the beginning of requestPermissionNeeded, so it doesn't
                            // take into account the response to the shown dialogs.
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_ONCE,
                                    /* expectedTimes= */ 1);
                        });
    }

    @Test
    public void
            testNotificationPrompt_showOSPromptAndReject_showRationaleAndAccept_rejectSecondOSPrompt() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            TestAndroidPermissionDelegate permissionDelegate =
                                    new TestAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, permissionDelegate);

                            // Show OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            // First attempt, should show OS prompt.
                            notificationPermissionController.requestPermissionIfNeeded();
                            long firstPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Reject the OS prompt.
                            invokeOSPermissionCallback(permissionDelegate, false);
                            // If the user hits "Deny" on the permission dialog then we have one
                            // more chance to ask again, so Android tells us to show a rationale
                            // first.
                            setShouldShowRequestPermissionRationale(activity, true);

                            // 10 days have passed since last request.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            // Set the rationale dialog to be approved, this should trigger a new OS
                            // prompt.
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

                            // Second attempt, should show rationale, then OS prompt.
                            notificationPermissionController.requestPermissionIfNeeded();
                            // Reject the OS prompt again.
                            invokeOSPermissionCallback(permissionDelegate, false);

                            long rationaleTimestamp =
                                    ContextUtils.getAppSharedPreferences()
                                            .getLong(
                                                    ChromePreferenceKeys
                                                            .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                                    0);
                            long secondPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // 15 days have passed since the second request.
                            mFakeTimeRule.advanceMillis(15 * DateUtils.DAY_IN_MILLIS);

                            // If our prompt gets rejected twice then we can't request permissions
                            // anymore.
                            setShouldShowRequestPermissionRationale(activity, false);

                            // Third attempt, nothing should happen.
                            notificationPermissionController.requestPermissionIfNeeded();

                            // We should have shown the rationale only once.
                            assertNotEquals(0, rationaleTimestamp);
                            assertEquals(1, rationaleDelegate.getCallCount());

                            // As the rationale was accepted we should have also shown the
                            // permission prompt twice.
                            assertNotEquals(0, firstPermissionRequestTimestamp);
                            assertNotEquals(0, secondPermissionRequestTimestamp);
                            assertTrue(
                                    secondPermissionRequestTimestamp
                                            > firstPermissionRequestTimestamp);

                            // If the user hits "Deny" again on the permission dialog then we can't
                            // request permissions anymore, the third call to
                            // requestPermissionIfNeeded() should not show anything.
                            assertEquals(
                                    secondPermissionRequestTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());

                            // We should record a histogram value indicating we've asked for
                            // permission 2 times, showing the rationale and the OS prompt in one
                            // call to requestPermissionIfNeeded counts as one attempt.
                            verifyPermissionRequestCountHistogram(/* expectedCount= */ 2);
                            // The third call to requestPermissionIfNeeded() should have recorded
                            // the state as "Asked twice, denied".
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_TWICE,
                                    /* expectedTimes= */ 1);
                        });
    }

    @Test
    public void testNotificationPrompt_showOSPromptAndReject_showRationaleAndReject() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, activity);

                            // Show OS prompt shown for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);

                            // Reject OS Prompt.
                            notificationPermissionController.requestPermissionIfNeeded();
                            setShouldShowRequestPermissionRationale(activity, true);

                            // Show for the second time after 10 days with rationale.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);

                            // Dismiss rationale.
                            Integer rationaleDialogAction =
                                    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;
                            rationaleDelegate.setDialogAction(rationaleDialogAction);
                            notificationPermissionController.requestPermissionIfNeeded();
                            long secondPermissionRequestTimestamp =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Try for a third time after 10 days. We shouldn't show.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);
                            notificationPermissionController.requestPermissionIfNeeded();

                            // The third call shouldn't have shown either rationale or permission
                            // dialog.
                            assertEquals(
                                    secondPermissionRequestTimestamp,
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp());
                            assertEquals(1, rationaleDelegate.getCallCount());
                            // The last call should have recorded the state as "Asked once, showed
                            // rationale then denied"
                            verifyStateHistogramWasRecorded(
                                    NotificationPermissionState.DENIED_ASKED_TWICE,
                                    /* expectedTimes= */ 1);
                        });
    }

    @Test
    public void testNotificationPrompt_usesWaitIntervalFromFieldTrialParams() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            TestRationaleDelegate rationaleDelegate = new TestRationaleDelegate();
                            NotificationPermissionController notificationPermissionController =
                                    createNotificationPermissionController(
                                            rationaleDelegate, activity);

                            // Set field trial params to wait 14 days.
                            setupFeatureParams(false, null, 14);

                            // Show and reject OS prompt for the first time.
                            setShouldShowRequestPermissionRationale(activity, false);
                            notificationPermissionController.requestPermissionIfNeeded();
                            setShouldShowRequestPermissionRationale(activity, true);
                            long requestTimestampAfterFirstStartup =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();

                            // Wait 10 days, nothing should happen yet.
                            mFakeTimeRule.advanceMillis(10 * DateUtils.DAY_IN_MILLIS);
                            notificationPermissionController.requestPermissionIfNeeded();

                            long requestTimestampAfterSecondStartup =
                                    PermissionPrefs
                                            .getAndroidNotificationPermissionRequestTimestamp();
                            int rationaleCallCountAfterSecondStartup =
                                    rationaleDelegate.getCallCount();

                            // Wait 5 more days, now we should show the rationale.
                            mFakeTimeRule.advanceMillis(5 * DateUtils.DAY_IN_MILLIS);
                            rationaleDelegate.setDialogAction(
                                    DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                            notificationPermissionController.requestPermissionIfNeeded();
                            int rationaleCallCountAfterThirdStartup =
                                    rationaleDelegate.getCallCount();

                            // The second call to requestPermissionIfNeeded shouldn't show a
                            // rationale or OS prompt.
                            assertEquals(
                                    requestTimestampAfterFirstStartup,
                                    requestTimestampAfterSecondStartup);
                            assertEquals(0, rationaleCallCountAfterSecondStartup);

                            // The third call should have shown the rationale.
                            assertEquals(1, rationaleCallCountAfterThirdStartup);
                        });
    }
}
