// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasCategories;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.hasItem;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING;
import static org.chromium.chrome.browser.pwd_migration.R.id.password_migration_more_options_button;
import static org.chromium.chrome.browser.pwd_migration.R.id.password_migration_next_button;
import static org.chromium.chrome.browser.pwd_migration.R.id.radio_password_export;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.widget.Button;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.HistogramExportResult;
import org.chromium.chrome.browser.password_manager.settings.ExportFlow;
import org.chromium.chrome.browser.password_manager.settings.ManualCallbackDelayer;
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.password_manager.settings.PasswordManagerHandlerProvider;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningCoordinator;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningTriggers;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

/** Tests for exports started from the local passwords migration warning. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
public class PasswordMigrationWarningExportFlowTest {
    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityRule = new ChromeTabbedActivityTestRule();

    private FakePasswordManagerHandler mFakePasswordManagerHandler;
    private PasswordMigrationWarningCoordinator mCoordinator;
    private ExportFlow mExportFlow;
    @Mock private PasswordStoreBridge mPasswordStoreBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mChromeActivityRule.startMainActivityOnBlankPage();
        Context context = mChromeActivityRule.getActivity();
        BottomSheetController bottomSheetController =
                mChromeActivityRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mExportFlow = new ExportFlow();
                    mFakePasswordManagerHandler =
                            new FakePasswordManagerHandler(
                                    PasswordManagerHandlerProvider.getForProfile(
                                            mChromeActivityRule.getProfile(false)));
                    // Create a password, otherwise the export will not be allowed when there are
                    // not passwords saved.
                    setPasswordSource("https://example.com", "test user", "password");
                    mCoordinator =
                            new PasswordMigrationWarningCoordinator(
                                    context,
                                    ProfileManager.getLastUsedRegularProfile(),
                                    bottomSheetController,
                                    SyncConsentActivityLauncherImpl.get(),
                                    ManageSyncSettings.class,
                                    mExportFlow,
                                    (PasswordListObserver observer) ->
                                            PasswordManagerHandlerProvider.getForProfile(
                                                            mChromeActivityRule.getProfile(false))
                                                    .addObserver(observer),
                                    mPasswordStoreBridge,
                                    PasswordMigrationWarningTriggers.CHROME_STARTUP,
                                    (Throwable exception) -> fail());
                    PasswordManagerHandlerProvider.getForProfile(
                                    mChromeActivityRule.getProfile(false))
                            .passwordListAvailable(1);
                    mCoordinator.showWarning();
                });
        // Go to the "More options" screen.
        onViewWaiting(allOf(withId(password_migration_more_options_button), isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button button =
                            mChromeActivityRule
                                    .getActivity()
                                    .findViewById(password_migration_more_options_button);
                    button.performClick();
                });
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/40925707")
    public void testExportIntent() throws Exception {
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        Intents.init();

        requestExport();

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                mExportFlow.getExportEventHistogramName(),
                                ExportFlow.PasswordExportEvent.EXPORT_CONFIRMED)
                        .expectIntRecord(
                                mExportFlow.getExportResultHistogramName2ForTesting(),
                                HistogramExportResult.SUCCESS)
                        .build();

        File tempFile = null;
        File outputFile = null;
        try {
            tempFile = createFakeExportedPasswordsFile();
            // Pretend that passwords have been serialized to go directly to the intent.
            mFakePasswordManagerHandler
                    .getExportSuccessCallback()
                    .onResult(123, tempFile.getPath());

            Intent result = new Intent();
            outputFile = createFakeSavedPasswordsFile();
            result.setData(FileUtils.getUriForFile(outputFile));
            // Pretend that user has chosen to save the passwords in the file system.
            intending(hasAction(Intent.ACTION_CREATE_DOCUMENT))
                    .respondWith(new ActivityResult(Activity.RESULT_OK, result));

            onViewWaiting(
                            allOf(
                                    withText(R.string.password_settings_export_action_title),
                                    isCompletelyDisplayed()),
                            /* checkRootDialog= */ true)
                    .perform(click());

            // Assert that the expected intent was detected.
            intended(
                    allOf(
                            hasAction(equalTo(Intent.ACTION_CREATE_DOCUMENT)),
                            hasCategories(hasItem(Intent.CATEGORY_OPENABLE)),
                            hasExtras(
                                    hasEntry(
                                            Intent.EXTRA_TITLE,
                                            equalTo(
                                                    mChromeActivityRule
                                                            .getActivity()
                                                            .getResources()
                                                            .getString(
                                                                    R.string
                                                                            .password_manager_default_export_filename)))),
                            hasType("text/csv")));

            // Assert that the output file was written.
            Assert.assertTrue(outputFile.length() > 0);
            histogram.assertExpected();
        } finally {
            if (tempFile != null) {
                tempFile.delete();
            }
            if (outputFile != null) {
                outputFile.delete();
            }
        }
        Intents.release();

        onViewWaiting(
                        allOf(
                                withText(R.string.exported_passwords_delete_button),
                                isCompletelyDisplayed()),
                        /* checkRootDialog= */ true)
                .perform(click());

        verify(mPasswordStoreBridge).clearAllPasswords();
    }

    /**
     * Check that metrics are logged when the export flow ends because there is no screen lock set
     * up.
     */
    @Test
    @MediumTest
    public void testExportFlowWithNoScreenLockRecordsMetrics() {
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        var exportResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                PasswordMigrationWarningCoordinator.EXPORT_METRICS_ID
                                        + PasswordMetricsUtil.EXPORT_RESULT_HISTOGRAM_SUFFIX,
                                PasswordMetricsUtil.HistogramExportResult.NO_SCREEN_LOCK_SET_UP)
                        .build();

        requestExport();

        exportResultHistogram.assertExpected();
    }

    /**
     * Selects the export option in the local passwords migration dialog and clicks next. After that
     * the export dialog is expected to be displayed and the export will be started. It also
     * disables the timer in DialogManager which is used to allow hiding the progress bar after an
     * initial period to avoid time-dependent flakiness.
     */
    private void requestExport() {
        ReauthenticationManager.setSkipSystemReauth(true);
        onViewWaiting(allOf(withId(radio_password_export), isCompletelyDisplayed()))
                .perform(click());
        onViewWaiting(allOf(withId(password_migration_next_button), isCompletelyDisplayed()))
                .perform(click());

        // Now Chrome thinks it triggered the challenge and is waiting to be resumed. Once resumed
        // it will check the reauthentication result. First, update the reauth timestamp to indicate
        // a successful reauth:
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Disable the timer for progress bar.
                    mExportFlow
                            .getDialogManagerForTesting()
                            .replaceCallbackDelayerForTesting(new ManualCallbackDelayer());
                    // Now call onResume to nudge Chrome into continuing the export flow.
                    mCoordinator.resumeExportFlow();
                });
    }

    private void setPasswordSource(String origin, String username, String password) {
        PasswordManagerHandlerProvider handlerProvider =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                PasswordManagerHandlerProvider.getForProfile(
                                        mChromeActivityRule.getProfile(false)));
        mFakePasswordManagerHandler.insertPasswordEntryForTesting(origin, username, password);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        handlerProvider.setPasswordManagerHandlerForTest(
                                mFakePasswordManagerHandler));
    }

    private File createFakeExportedPasswordsFile() throws IOException {
        File passwordsDir = new File(ExportFlow.getTargetDirectory());
        // Ensure that the directory exists.
        passwordsDir.mkdir();
        File tempFile = File.createTempFile("test", ".csv", passwordsDir);
        FileWriter writer = new FileWriter(tempFile);
        writer.write("Fake serialized passwords");

        writer.close();
        return tempFile;
    }

    private File createFakeSavedPasswordsFile() throws IOException {
        File outputFile = new File(ExportFlow.getTargetDirectory(), "test_saved_passwords.csv");
        outputFile.createNewFile();
        return outputFile;
    }
}
