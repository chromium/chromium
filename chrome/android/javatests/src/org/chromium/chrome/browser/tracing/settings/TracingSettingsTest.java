// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing.settings;

import static android.app.Notification.FLAG_ONGOING_EVENT;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.core.app.NotificationCompat;
import androidx.preference.CheckBoxPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tracing.TracingController;
import org.chromium.chrome.browser.tracing.TracingNotificationManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.settings.ButtonPreference;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.TextMessagePreference;

import java.io.File;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for the Tracing settings menu. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TracingSettingsTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final SettingsActivityTestRule<TracingSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(TracingSettings.class);

    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() {
        mMockNotificationManager = new MockNotificationManagerProxy();
        TracingNotificationManager.overrideNotificationManagerForTesting(mMockNotificationManager);
    }

    @After
    public void tearDown() {
        TracingNotificationManager.overrideNotificationManagerForTesting(null);
    }

    /**
     * Waits until a notification has been displayed and then returns a NotificationEntry object to
     * the caller. Requires that only a single notification is displayed.
     *
     * @return The NotificationEntry object tracked by the MockNotificationManagerProxy.
     */
    private MockNotificationManagerProxy.NotificationEntry waitForNotification() {
        waitForNotificationManagerMutation();
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        Assert.assertEquals(1, notifications.size());
        return notifications.get(0);
    }

    /**
     * Waits for a mutation to occur in the mocked notification manager. This indicates that Chrome
     * called into Android to notify or cancel a notification.
     */
    private void waitForNotificationManagerMutation() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mMockNotificationManager.getMutationCountAndDecrement(),
                            Matchers.greaterThan(0));
                },
                /* maxTimeoutMs= */ 15000L,
                /* checkIntervalMs= */ 50);
    }

    private void waitForTracingControllerInitialization(PreferenceFragmentCompat fragment)
            throws Exception {
        final Preference defaultCategoriesPref =
                fragment.findPreference(TracingSettings.UI_PREF_DEFAULT_CATEGORIES);
        final Preference nonDefaultCategoriesPref =
                fragment.findPreference(TracingSettings.UI_PREF_NON_DEFAULT_CATEGORIES);
        final Preference modePref = fragment.findPreference(TracingSettings.UI_PREF_MODE);
        final Preference startTracingButton =
                fragment.findPreference(TracingSettings.UI_PREF_START_RECORDING);

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (TracingController.getInstance().getState()
                            == TracingController.State.INITIALIZING) {
                        // Controls should be disabled while initializing.
                        Assert.assertFalse(defaultCategoriesPref.isEnabled());
                        Assert.assertFalse(nonDefaultCategoriesPref.isEnabled());
                        Assert.assertFalse(modePref.isEnabled());
                        Assert.assertFalse(startTracingButton.isEnabled());

                        TracingController.getInstance()
                                .addObserver(
                                        new TracingController.Observer() {
                                            @Override
                                            public void onTracingStateChanged(
                                                    @TracingController.State int state) {
                                                callbackHelper.notifyCalled();
                                                TracingController.getInstance()
                                                        .removeObserver(this);
                                            }
                                        });
                        return;
                    }
                    // Already initialized.
                    callbackHelper.notifyCalled();
                });
        callbackHelper.waitForCallback(/* currentCallCount= */ 0);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testRecordTrace() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSettingsActivityTestRule.startSettingsActivity();
        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final ButtonPreference startTracingButton =
                (ButtonPreference) fragment.findPreference(TracingSettings.UI_PREF_START_RECORDING);
        final ButtonPreference shareTraceButton =
                (ButtonPreference) fragment.findPreference(TracingSettings.UI_PREF_SHARE_TRACE);

        waitForTracingControllerInitialization(fragment);

        // Setting to IDLE state tries to dismiss the (non-existent) notification.
        waitForNotificationManagerMutation();
        Assert.assertEquals(0, mMockNotificationManager.getNotifications().size());

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            TracingController.State.IDLE,
                            TracingController.getInstance().getState());
                    Assert.assertTrue(startTracingButton.isEnabled());
                    Assert.assertEquals(TracingSettings.MSG_START, startTracingButton.getTitle());
                    Assert.assertFalse(shareTraceButton.isEnabled());

                    // Tap the button to start recording a trace.
                    startTracingButton
                            .getOnPreferenceClickListener()
                            .onPreferenceClick(startTracingButton);

                    Assert.assertEquals(
                            TracingController.State.STARTING,
                            TracingController.getInstance().getState());
                    Assert.assertFalse(startTracingButton.isEnabled());
                    Assert.assertEquals(TracingSettings.MSG_ACTIVE, startTracingButton.getTitle());

                    // Observe state changes to RECORDING, STOPPING, STOPPED, and IDLE.
                    TracingController.getInstance()
                            .addObserver(
                                    new TracingController.Observer() {
                                        @TracingController.State
                                        int mExpectedState = TracingController.State.RECORDING;

                                        @Override
                                        public void onTracingStateChanged(
                                                @TracingController.State int state) {
                                            // onTracingStateChanged() should be called four times
                                            // in total, with the right order of state changes:
                                            Assert.assertEquals(mExpectedState, state);
                                            if (state == TracingController.State.RECORDING) {
                                                mExpectedState = TracingController.State.STOPPING;
                                            } else if (state == TracingController.State.STOPPING) {
                                                mExpectedState = TracingController.State.STOPPED;
                                            } else if (state == TracingController.State.STOPPED) {
                                                mExpectedState = TracingController.State.IDLE;
                                            } else {
                                                TracingController.getInstance()
                                                        .removeObserver(this);
                                            }

                                            callbackHelper.notifyCalled();
                                        }
                                    });
                });

        // Wait for state change to RECORDING.
        callbackHelper.waitForCallback(/* currentCallCount= */ 0);

        // Recording started, a notification with a stop button should be displayed.
        Notification notification = waitForNotification().notification;
        Assert.assertEquals(FLAG_ONGOING_EVENT, notification.flags & FLAG_ONGOING_EVENT);
        Assert.assertEquals(null, notification.deleteIntent);
        Assert.assertEquals(1, NotificationCompat.getActionCount(notification));
        PendingIntent stopIntent = NotificationCompat.getAction(notification, 0).actionIntent;

        // Initiate stopping the recording and wait for state changes to STOPPING and STOPPED.
        stopIntent.send();
        callbackHelper.waitForCallback(
                /* currentCallCount= */ 1,
                /* numberOfCallsToWaitFor= */ 2,
                /* timeout= */ 15000L,
                TimeUnit.MILLISECONDS);

        // Notification should be replaced twice, once with an "is stopping" notification and then
        // with a notification to share the trace. Because the former is temporary, we can't
        // verify it reliably, so we skip over it and simply expect two notification mutations.
        waitForNotification();
        notification = waitForNotification().notification;
        Assert.assertEquals(0, notification.flags & FLAG_ONGOING_EVENT);
        Assert.assertNotEquals(null, notification.deleteIntent);
        PendingIntent deleteIntent = notification.deleteIntent;

        // The temporary tracing output file should now exist and the "share trace" button
        // should be active.
        File tempFile = TracingController.getInstance().getTracingTempFile();
        Assert.assertNotEquals(null, tempFile);
        Assert.assertTrue(tempFile.exists());
        Assert.assertTrue(shareTraceButton.isEnabled());

        // Discard the trace and wait for state change back to IDLE.
        deleteIntent.send();
        callbackHelper.waitForCallback(/* currentCallCount= */ 3);

        // The temporary file should be deleted asynchronously.
        CriteriaHelper.pollInstrumentationThread(() -> !tempFile.exists());

        // Notification should be deleted, too.
        waitForNotificationManagerMutation();
        Assert.assertEquals(0, mMockNotificationManager.getNotifications().size());

        // The share trace button should be disabled again.
        Assert.assertFalse(shareTraceButton.isEnabled());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testNotificationsDisabledMessage() throws Exception {
        mMockNotificationManager.setNotificationsEnabled(false);

        mSettingsActivityTestRule.startSettingsActivity();
        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final ButtonPreference startTracingButton =
                (ButtonPreference) fragment.findPreference(TracingSettings.UI_PREF_START_RECORDING);
        final TextMessagePreference statusPreference =
                (TextMessagePreference)
                        fragment.findPreference(TracingSettings.UI_PREF_TRACING_STATUS);

        waitForTracingControllerInitialization(fragment);

        Assert.assertFalse(startTracingButton.isEnabled());
        Assert.assertEquals(
                TracingSettings.MSG_NOTIFICATIONS_DISABLED, statusPreference.getTitle());

        mMockNotificationManager.setNotificationsEnabled(true);
    }

    public static class CategoryParams implements ParameterProvider {
        private static List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(TracingSettings.UI_PREF_DEFAULT_CATEGORIES, "toplevel")
                                .name("DefaultCategories"),
                        new ParameterSet()
                                .value(
                                        TracingSettings.UI_PREF_NON_DEFAULT_CATEGORIES,
                                        "disabled-by-default-cc.debug")
                                .name("NonDefaultCategories"));

        @Override
        public List<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @ParameterAnnotations.UseMethodParameter(CategoryParams.class)
    public void testSelectCategories(String preferenceKey, String sampleCategoryName)
            throws Exception {
        // We need a renderer so that its tracing categories will be populated.
        mActivityTestRule.startMainActivityOnBlankPage();
        mSettingsActivityTestRule.startSettingsActivity();
        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final Preference categoriesPref = fragment.findPreference(preferenceKey);

        waitForTracingControllerInitialization(fragment);

        ThreadUtils.runOnUiThreadBlocking(() -> Assert.assertTrue(categoriesPref.isEnabled()));

        // Simulate clicking the preference, which should open a new preferences fragment in
        // a new activity.
        Context context = ApplicationProvider.getApplicationContext();
        Assert.assertNotNull(categoriesPref.getExtras());
        Assert.assertFalse(categoriesPref.getExtras().isEmpty());
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context, TracingCategoriesSettings.class, categoriesPref.getExtras());
        mSettingsActivityTestRule.launchActivity(intent);
        SettingsActivity categoriesActivity = mSettingsActivityTestRule.getActivity();

        PreferenceFragmentCompat categoriesFragment =
                (PreferenceFragmentCompat) categoriesActivity.getMainFragment();
        Assert.assertEquals(TracingCategoriesSettings.class, categoriesFragment.getClass());

        CheckBoxPreference sampleCategoryPref =
                (CheckBoxPreference) categoriesFragment.findPreference(sampleCategoryName);
        Assert.assertNotNull(sampleCategoryPref);

        boolean originallyEnabled =
                TracingSettings.getEnabledCategories().contains(sampleCategoryName);
        Assert.assertEquals(originallyEnabled, sampleCategoryPref.isChecked());

        // Simulate selecting / deselecting the category.
        ThreadUtils.runOnUiThreadBlocking(sampleCategoryPref::performClick);
        Assert.assertNotEquals(originallyEnabled, sampleCategoryPref.isChecked());
        boolean finallyEnabled =
                TracingSettings.getEnabledCategories().contains(sampleCategoryName);
        Assert.assertNotEquals(originallyEnabled, finallyEnabled);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSelectMode() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();
        final PreferenceFragmentCompat fragment = mSettingsActivityTestRule.getFragment();
        final ListPreference modePref =
                (ListPreference) fragment.findPreference(TracingSettings.UI_PREF_MODE);

        waitForTracingControllerInitialization(fragment);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(modePref.isEnabled());

                    // By default, the "record-until-full" mode is selected.
                    Assert.assertEquals(
                            "record-until-full", TracingSettings.getSelectedTracingMode());

                    // Dialog should contain 3 entries.
                    Assert.assertEquals(3, modePref.getEntries().length);

                    // Simulate changing the mode.
                    modePref.getOnPreferenceChangeListener()
                            .onPreferenceChange(modePref, "record-continuously");
                    Assert.assertEquals(
                            "record-continuously", TracingSettings.getSelectedTracingMode());
                });
    }
}
