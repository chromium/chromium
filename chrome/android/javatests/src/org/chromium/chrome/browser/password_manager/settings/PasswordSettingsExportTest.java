// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasCategories;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasHost;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING;
import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PASSWORD_SETTINGS_EXPORT_METRICS_ID;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.HistogramExportResult;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

/** Tests for exports started at the "Passwords" settings screen. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests are flaky on API Q+ with batching. This might be fixable. b/40926377")
public class PasswordSettingsExportTest {
    @Rule
    public SettingsActivityTestRule<PasswordSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PasswordSettings.class);

    @Mock private PasswordCheck mPasswordCheck;

    private final PasswordSettingsTestHelper mTestHelper = new PasswordSettingsTestHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
    }

    @After
    public void tearDown() {
        mTestHelper.tearDown();
    }

    /** Check that if there are no saved passwords, the export menu item is disabled. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuDisabled() {
        // Ensure there are no saved passwords reported to settings.
        mTestHelper.setPasswordSource(null);

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        checkExportMenuItemState(false);
    }

    /** Check that if there are saved passwords, the export menu item is enabled. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuEnabled() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        checkExportMenuItemState(true);
    }

    /**
     * Check that tapping the export menu requests the passwords to be serialised in the background.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportTriggersSerialization() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        mSettingsActivityTestRule
                                .getFragment()
                                .getExportFlowForTesting()
                                .getExportEventHistogramName(),
                        ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED);

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Before tapping the menu item for export, pretend that the last successful
        // reauthentication just happened. This will allow the export flow to continue.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        Assert.assertNotNull(mTestHelper.getHandler().getExportTargetPath());
        Assert.assertFalse(mTestHelper.getHandler().getExportTargetPath().isEmpty());
        histogram.assertExpected();
    }

    /**
     * Check that the export menu item is included and hidden behind the overflow menu. Check that
     * the menu displays the warning before letting the user export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItem() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Check that the warning dialog is displayed.
        onView(withText(R.string.settings_passwords_export_description))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));
    }

    /**
     * Check that if export is canceled by the user after a successful reauthentication, then
     * re-triggering the export and failing the second reauthentication aborts the export as well.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportReauthAfterCancel() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Hit the Cancel button on the warning dialog to cancel the flow.
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        // Now repeat the steps almost like in |reauthenticateAndRequestExport| but simulate failing
        // the reauthentication challenge.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        // Now Chrome thinks it triggered the challenge and is waiting to be resumed. Once resumed
        // it will check the reauthentication result. First, update the reauth timestamp to indicate
        // a cancelled reauth:
        ReauthenticationManager.resetLastReauth();

        // Now call onResume to nudge Chrome into continuing the export flow.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsActivity.getMainFragment().onResume();
                });

        // Check that the warning dialog is not displayed.
        onView(withText(R.string.settings_passwords_export_description)).check(doesNotExist());

        // Check that the export menu item is enabled, because the current export was cancelled.
        checkExportMenuItemState(true);
    }

    /**
     * Check that metrics are recorded when export flow is aborted because the screen lock is not
     * set up.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    public void testExportFlowWithNoScreenLockRecordsMetrics() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        var exportResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PASSWORD_SETTINGS_EXPORT_METRICS_ID
                                        + PasswordMetricsUtil.EXPORT_RESULT_HISTOGRAM_SUFFIX,
                                PasswordMetricsUtil.HistogramExportResult.NO_SCREEN_LOCK_SET_UP)
                        .build();

        reauthenticateAndRequestExport(settingsActivity);

        exportResultHistogram.assertExpected();
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemNoLock() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        View mainDecorView = settingsActivity.getWindow().getDecorView();
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());
        onView(withText(R.string.password_export_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isCompletelyDisplayed()));
    }

    /**
     * Check that if exporting is cancelled for the absence of the screen lock, the menu item is
     * enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemReenabledNoLock() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Trigger exporting and let it fail on the unavailable lock.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        // Check that for re-triggering, the export menu item is enabled.
        checkExportMenuItemState(true);
    }

    /**
     * Check that if exporting is cancelled for the user's failure to reauthenticate, the menu item
     * is enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemReenabledReauthFailure() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setSkipSystemReauth(true);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());
        // The reauthentication dialog is skipped and the last reauthentication timestamp is not
        // reset. This looks like a failed reauthentication to PasswordSettings' onResume.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsActivity.getMainFragment().onResume();
                });
        checkExportMenuItemState(true);
    }

    /**
     * Check that the export always requires a reauthentication, even if the last one happened
     * recently.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportRequiresReauth() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        // Ensure that the last reauthentication time stamp is recent enough.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        // Start export.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        // Check that Chrome indeed issued an (ignored) request to reauthenticate the user rather
        // than re-using the recent reauthentication, by observing that the next step in the flow
        // (progress bar) is not shown.
        onView(withText(R.string.settings_passwords_preparing_export)).check(doesNotExist());
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords.
     */
    @Test
    @SmallTest
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    @Feature({"Preferences"})
    public void testExportIntent() throws Exception {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        Intents.init();

        var exportEventHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                mSettingsActivityTestRule
                                        .getFragment()
                                        .getExportFlowForTesting()
                                        .getExportEventHistogramName(),
                                ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED,
                                ExportFlow.PasswordExportEvent.EXPORT_CONFIRMED)
                        .build();

        reauthenticateAndRequestExport(settingsActivity);
        File tempFile = createFakeExportedPasswordsFile();
        // Pretend that passwords have been serialized to go directly to the intent.
        mTestHelper.getHandler().getExportSuccessCallback().onResult(123, tempFile.getPath());

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());
        exportEventHistogram.assertExpected();

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /** Check that the export flow ends with saving the file with passwords to the file system. */
    @Test
    @SmallTest
    @EnableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    @Feature({"Preferences"})
    public void testExportToDownloadsIntent() throws Exception {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                mSettingsActivityTestRule
                                        .getFragment()
                                        .getExportFlowForTesting()
                                        .getExportEventHistogramName(),
                                ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED,
                                ExportFlow.PasswordExportEvent.EXPORT_CONFIRMED)
                        .build();

        Intents.init();

        reauthenticateAndRequestExport(settingsActivity);
        File tempFile = createFakeExportedPasswordsFile();
        // Pretend that passwords have been serialized to go directly to the intent.
        mTestHelper.getHandler().getExportSuccessCallback().onResult(123, tempFile.getPath());

        // Simulate that the intent would return a newly created file.
        Intent result = new Intent();
        File outputFile = createFakeSavedPasswordsFile();
        result.setData(FileUtils.getUriForFile(outputFile));
        // Pretend that user has chosen to save the passwords in the file system.
        intending(hasAction(Intent.ACTION_CREATE_DOCUMENT))
                .respondWith(new ActivityResult(Activity.RESULT_OK, result));

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());
        histogram.assertExpected();

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CREATE_DOCUMENT)),
                        hasCategories(hasItem(Intent.CATEGORY_OPENABLE)),
                        hasExtras(hasEntry(Intent.EXTRA_TITLE, Matchers.notNullValue())),
                        hasType("text/csv")));
        // Assert that the output file was written.
        Assert.assertTrue(outputFile.length() > 0);

        Intents.release();

        tempFile.delete();
        outputFile.delete();
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords, even if the flow gets interrupted by pausing Chrome.
     */
    @Test
    @SmallTest
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    @Feature({"Preferences"})
    public void testExportIntentPaused() throws Exception {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        Intents.init();

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                mSettingsActivityTestRule
                                        .getFragment()
                                        .getExportFlowForTesting()
                                        .getExportEventHistogramName(),
                                ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED,
                                ExportFlow.PasswordExportEvent.EXPORT_CONFIRMED)
                        .build();

        reauthenticateAndRequestExport(settingsActivity);

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsActivity.getMainFragment().onResume();
                });

        File tempFile = createFakeExportedPasswordsFile();
        // Pretend that passwords have been serialized to go directly to the intent.
        mTestHelper.getHandler().getExportSuccessCallback().onResult(56, tempFile.getPath());

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());
        histogram.assertExpected();

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that the export flow can be canceled in the warning dialogue and that upon cancellation
     * the export menu item gets re-enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnWarning() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                mSettingsActivityTestRule
                                        .getFragment()
                                        .getExportFlowForTesting()
                                        .getExportEventHistogramName(),
                                ExportFlow.PasswordExportEvent.EXPORT_OPTION_SELECTED,
                                ExportFlow.PasswordExportEvent.EXPORT_DISMISSED)
                        .build();

        reauthenticateAndRequestExport(settingsActivity);

        // Cancel the export warning.
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(true);
        histogram.assertExpected();
    }

    /** Check that the export warning is not duplicated when onResume is called on the settings. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportWarningOnResume() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsActivity.getMainFragment().onResume();
                });

        // Cancel the export warning.
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        // Check that export warning is not visible again.
        onView(withText(R.string.cancel)).check(doesNotExist());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(true);
    }

    /**
     * Check that the export warning is dismissed after onResume if the last reauthentication
     * happened too long ago.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportWarningTimeoutOnResume() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Before exporting, pretend that the last successful reauthentication happened too long
        // ago.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis()
                        - ReauthenticationManager.VALID_REAUTHENTICATION_TIME_INTERVAL_MILLIS
                        - 1,
                ReauthenticationManager.ReauthScope.BULK);

        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    settingsActivity.getMainFragment().onResume();
                });

        // Check that export warning is not visible again.
        onView(withText(R.string.cancel)).check(doesNotExist());

        // Check that the export flow was cancelled automatically by checking that the export menu
        // is available and enabled.
        checkExportMenuItemState(true);
    }

    /**
     * Check that the export flow can be canceled by dismissing the warning dialogue (tapping
     * outside of it, as opposed to tapping "Cancel") and that upon cancellation the export menu
     * item gets re-enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnWarningDismissal() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Verify that the warning dialog is shown and then dismiss it through pressing back (as
        // opposed to the cancel button).
        onView(withText(R.string.password_settings_export_action_title))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));
        Espresso.pressBack();

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(true);
    }

    /** Check that a progressbar is displayed for a minimal time duration to avoid flickering. */
    @Test
    @SmallTest
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    @Feature({"Preferences"})
    public void testExportProgressMinimalTime() throws Exception {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        Intents.init();

        // This also disables the timer for keeping the progress bar up. The test can thus emulate
        // that timer going off by calling {@link allowProgressBarToBeHidden}.
        reauthenticateAndRequestExport(settingsActivity);

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Before simulating the serialized passwords being received, check that the progress bar is
        // shown.
        onView(withText(R.string.settings_passwords_preparing_export))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));

        File tempFile = createFakeExportedPasswordsFile();
        // Now pretend that passwords have been serialized.
        mTestHelper.getHandler().getExportSuccessCallback().onResult(12, tempFile.getPath());

        // Check that the progress bar is still shown, though, because the timer has not gone off
        // yet.
        onView(withText(R.string.settings_passwords_preparing_export))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));

        // Now mark the timer as gone off and check that the progress bar is hidden.
        allowProgressBarToBeHidden();
        onView(withText(R.string.settings_passwords_preparing_export)).check(doesNotExist());

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that a progress bar is displayed when the user confirms the export and the serialized
     * passwords are not ready yet.
     */
    @Test
    @SmallTest
    @DisableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    @Feature({"Preferences"})
    public void testExportProgress() throws Exception {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        Intents.init();

        reauthenticateAndRequestExport(settingsActivity);

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Before simulating the serialized passwords being received, check that the progress bar is
        // shown.
        onViewWaiting(
                        withText(R.string.settings_passwords_preparing_export),
                        /* checkRootDialog= */ true)
                .check(matches(isCompletelyDisplayed()));

        File tempFile = createFakeExportedPasswordsFile();

        // Now pretend that passwords have been serialized.
        allowProgressBarToBeHidden();
        mTestHelper.getHandler().getExportSuccessCallback().onResult(12, tempFile.getPath());

        // After simulating the serialized passwords being received, check that the progress bar is
        // hidden.
        onView(withText(R.string.settings_passwords_preparing_export)).check(doesNotExist());

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /** Check that the user can cancel exporting with the "Cancel" button on the progressbar. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnProgress() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning to fire the sharing intent.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Simulate the minimal time for showing the progress bar to have passed, to ensure that it
        // is kept live because of the pending serialization.
        allowProgressBarToBeHidden();

        // Check that the progress bar is shown.
        onView(withText(R.string.settings_passwords_preparing_export))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));

        // Hit the Cancel button.
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(true);
    }

    /** Check that the user can cancel exporting with the negative button on the error message. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnError() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Show an arbitrary error. This should replace the progress bar if that has been shown in
        // the meantime.
        allowProgressBarToBeHidden();
        requestShowingExportError();

        // Check that the error prompt is showing.
        onView(withText(R.string.password_settings_export_error_title))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));

        // Hit the negative button on the error prompt.
        onView(withText(R.string.close)).inRoot(isDialog()).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(true);
    }

    /**
     * Check that the user can re-trigger the export from an error dialog which has a "retry"
     * button.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportRetry() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Show an arbitrary error but ensure that the positive button label is the one for "try
        // again".
        allowProgressBarToBeHidden();
        requestShowingExportErrorWithButton(R.string.try_again);

        // Hit the positive button to try again.
        onView(withText(R.string.try_again)).inRoot(isDialog()).perform(click());

        // Check that there is again the export warning.
        onView(withText(R.string.password_settings_export_action_title))
                .inRoot(isDialog())
                .check(matches(isCompletelyDisplayed()));
    }

    /**
     * Check that the error dialog lets the user visit a help page to install Google Drive if they
     * need an app to consume the exported passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportHelpSite() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        true)
                .perform(click());

        // Show an arbitrary error but ensure that the positive button label is the one for the
        // Google Drive help site.
        allowProgressBarToBeHidden();
        requestShowingExportErrorWithButton(R.string.password_settings_export_learn_google_drive);

        Intents.init();

        // Before triggering the viewing intent, stub it out to avoid cascading that into further
        // intents and opening the web browser.
        intending(hasAction(equalTo(Intent.ACTION_VIEW)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Hit the positive button to navigate to the help site.
        onView(withText(R.string.password_settings_export_learn_google_drive))
                .inRoot(isDialog())
                .perform(click());

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_VIEW)),
                        hasData(hasHost(equalTo("support.google.com")))));

        Intents.release();
    }

    /**
     * Check that if errors are encountered when user is busy confirming the export, the error UI is
     * shown after the confirmation is completed, so that the user does not see UI changing
     * unexpectedly under their fingers.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportErrorUiAfterConfirmation() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        reauthenticateAndRequestExport(settingsActivity);

        // Request showing an arbitrary error while the confirmation dialog is still up.
        requestShowingExportError();

        // Check that the confirmation dialog is showing and dismiss it.
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        // Check that now the error is displayed, instead of the progress bar.
        allowProgressBarToBeHidden();
        onView(withText(R.string.settings_passwords_preparing_export)).check(doesNotExist());
        onView(withText(R.string.password_settings_export_error_title))
                .check(matches(isCompletelyDisplayed()));

        // Close the error dialog and abort the export.
        onView(withText(R.string.close)).perform(click());

        // Ensure that there is still no progress bar.
        onView(withText(R.string.settings_passwords_preparing_export)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testDontRepeatedlySerialisePasswords() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity =
                mTestHelper.startPasswordSettingsFromMainSettings(mSettingsActivityTestRule);

        PasswordSettings fragment = mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ExportFlow exportFlow = fragment.getExportFlowForTesting();
                    exportFlow.startExporting();
                    exportFlow.passwordsAvailable();
                    exportFlow.passwordsAvailable();
                });

        Assert.assertEquals(1, mTestHelper.getHandler().getSerializationInvocationCount());
    }

    /**
     * Taps the menu item to trigger exporting and ensures that reauthentication passes. It also
     * disables the timer in {@link DialogManager} which is used to allow hiding the progress bar
     * after an initial period. Hiding can be later allowed manually in tests with {@link
     * #allowProgressBarToBeHidden}, to avoid time-dependent flakiness.
     */
    private void reauthenticateAndRequestExport(SettingsActivity settingsActivity) {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        onViewWaiting(
                        allOf(
                                withText(R.string.password_settings_export_action_title),
                                isCompletelyDisplayed()))
                .perform(click());

        // Now Chrome thinks it triggered the challenge and is waiting to be resumed. Once resumed
        // it will check the reauthentication result. First, update the reauth timestamp to indicate
        // a successful reauth:
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Disable the timer for progress bar.
                    PasswordSettings fragment = mSettingsActivityTestRule.getFragment();
                    fragment.getExportFlowForTesting()
                            .getDialogManagerForTesting()
                            .replaceCallbackDelayerForTesting(mTestHelper.getManualDelayer());
                    // Now call onResume to nudge Chrome into continuing the export flow.
                    settingsActivity.getMainFragment().onResume();
                });
    }

    /**
     * Checks that the menu item for exporting passwords is enabled or disabled as expected.
     *
     * @param isMenuItemStateEnabled The expected state of the menu item.
     */
    private void checkExportMenuItemState(boolean isMenuItemStateEnabled) {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Matches a TextView, but the disabled entity is a wrapper higher in the menu's hierarchy.
        final Matcher<View> isDescendantOfDisabledParent = isDescendantOfA(not(isEnabled()));
        onViewWaiting(withText(R.string.password_settings_export_action_title))
                .check(
                        matches(
                                isMenuItemStateEnabled
                                        ? not(isDescendantOfDisabledParent)
                                        : isDescendantOfDisabledParent));
    }

    /** Requests showing an arbitrary password export error. */
    private void requestShowingExportError() {
        ThreadUtils.runOnUiThreadBlocking(
                mTestHelper.getHandler().getExportErrorCallback().bind("Arbitrary error"));
    }

    /**
     * Requests showing an arbitrary password export error with a particular positive button to be
     * shown. If you don't care about the button, just call {@link #requestShowingExportError}.
     *
     * @param positiveButtonLabelId controls which label the positive button ends up having.
     */
    private void requestShowingExportErrorWithButton(int positiveButtonLabelId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings fragment = mSettingsActivityTestRule.getFragment();
                    // To show an error, the error type for UMA needs to be specified. Because it is
                    // not relevant for cases when the error is forcibly displayed in tests,
                    // HistogramExportResult.NO_CONSUMER is passed as an arbitrarily chosen value.
                    fragment.getExportFlowForTesting()
                            .showExportErrorAndAbort(
                                    R.string.password_settings_export_no_app,
                                    null,
                                    positiveButtonLabelId,
                                    HistogramExportResult.NO_CONSUMER);
                });
    }

    /**
     * Sends the signal to {@link DialogManager} that the minimal time for showing the progress bar
     * has passed. This results in the progress bar getting hidden as soon as requested.
     */
    private void allowProgressBarToBeHidden() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestHelper.getManualDelayer().runCallbacksSynchronously();
                });
    }

    /**
     * Create a temporary file in the cache sub-directory for exported passwords, which the test can
     * try to use for sharing.
     *
     * @return The {@link File} handle for such temporary file.
     */
    private static File createFakeExportedPasswordsFile() throws IOException {
        File passwordsDir = new File(ExportFlow.getTargetDirectory());
        // Ensure that the directory exists.
        passwordsDir.mkdir();
        File tempFile = File.createTempFile("test", ".csv", passwordsDir);
        FileWriter writer = new FileWriter(tempFile);
        writer.write("Fake serialized passwords");

        writer.close();
        return tempFile;
    }

    /**
     * Creates an empty file, which can be used as the result of ACTION_CREATE_DOCUMENT intent.
     *
     * @return The newly created empty file.
     */
    private File createFakeSavedPasswordsFile() throws IOException {
        File passwordsDir = new File(ExportFlow.getTargetDirectory());
        // Ensure that the directory exists.
        passwordsDir.mkdir();
        File outputFile = new File(ExportFlow.getTargetDirectory(), "test_saved_passwords.csv");
        outputFile.createNewFile();
        return outputFile;
    }
}
