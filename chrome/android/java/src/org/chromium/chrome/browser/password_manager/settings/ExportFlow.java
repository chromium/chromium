// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class allows to trigger and complete the UX flow for exporting passwords. A {@link Fragment}
 * can use it to display the flow UI over the fragment.
 *
 * Internally, the flow is represented by the following calls:
 * (1)  {@link #startExporting}, which triggers both preparing of stored passwords in the background
 *      and reauthentication of the user.
 * (2a) {@link #shareSerializedPasswords}, which is the final part of the preparation of passwords
 *      which otherwise runs in the native code.
 * (2b) {@link #exportAfterReauth} is the user-visible next step after reauthentication. It displays
 *      a warning dialog, requesting the user to confirm that they indeed want to export the
 *      passwords.
 * (3)  {@link #tryExporting} merges the flow of the in-parallel-running (2a) and (2b). In the rare
 *      case when (2b) finishes before (2a), it also displays a progress bar.
 * (4)  {@link #sendExportIntent} creates an intent chooser for sharing the exported passwords with
 *      an app of user's choice.
 */
public class ExportFlow {
    @IntDef({ExportState.INACTIVE, ExportState.REQUESTED, ExportState.CONFIRMED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ExportState {
        /**
         * INACTIVE: there is no currently running export. Either the user did not request
         * one, or the last one completed (i.e., a share intent picker or an error message were
         * displayed or the user cancelled it).
         */
        int INACTIVE = 0;
        /**
         * REQUESTED: the user requested the export in the menu but did not authenticate
         * and confirm it yet.
         */
        int REQUESTED = 1;
        /**
         * CONFIRMED: the user confirmed the export and Chrome is still busy preparing the
         * data for the share intent.
         */
        int CONFIRMED = 2;
    }

    /** Describes at which state the password export flow is. */
    @ExportState
    private int mExportState;

    /** Name of the subdirectory in cache which stores the exported passwords file. */
    private static final String PASSWORDS_CACHE_DIR = "/passwords";

    /** The key for saving {@link #mExportState} to instance bundle. */
    private static final String SAVED_STATE_EXPORT_STATE = "saved-state-export-state";

    /** The key for saving {@link #mEntriesCount}|to instance bundle. */
    private static final String SAVED_STATE_ENTRIES_COUNT = "saved-state-entries-count";

    /** The key for saving {@link #mExportFileUri} to instance bundle. */
    private static final String SAVED_STATE_EXPORT_FILE_URI = "saved-state-export-file-uri";

    // Potential values of the histogram recording the result of exporting. This needs to match
    // ExportPasswordsResult from
    // //components/password_manager/core/browser/password_manager_metrics_util.h.
    @IntDef({HistogramExportResult.SUCCESS, HistogramExportResult.USER_ABORTED,
            HistogramExportResult.WRITE_FAILED, HistogramExportResult.NO_CONSUMER})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface HistogramExportResult {
        @VisibleForTesting
        int SUCCESS = 0;
        @VisibleForTesting
        int USER_ABORTED = 1;
        @VisibleForTesting
        int WRITE_FAILED = 2;
        @VisibleForTesting
        int NO_CONSUMER = 3;
        // If you add new values to HistogramExportResult, also update NUM_ENTRIES to match
        // its new size.
        int NUM_ENTRIES = 4;
    }

    // Values of the histogram recording password export related events.
    @IntDef({PasswordExportEvent.EXPORT_OPTION_SELECTED, PasswordExportEvent.EXPORT_DISMISSED,
            PasswordExportEvent.EXPORT_CONFIRMED, PasswordExportEvent.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordExportEvent {
        int EXPORT_OPTION_SELECTED = 0;
        int EXPORT_DISMISSED = 1;
        int EXPORT_CONFIRMED = 2;
        int COUNT = 3;
    }

    /**
     * When the user requests that passwords are exported and once the passwords are sent over from
     * native code and stored in a cache file, this variable contains the content:// URI for that
     * cache file, or an empty URI if there was a problem with storing to that file. During all
     * other times, this variable is null. In particular, after the export is requested, the
     * variable being null means that the passwords have not arrived from the native code yet.
     */
    @Nullable
    private Uri mExportFileUri;

    /**
     * The number of password entries contained in the most recent serialized data for password
     * export. The null value indicates that serialization has not completed since the last request
     * (or there was no request at all).
     */
    @Nullable
    private Integer mEntriesCount;

    // Histogram values for "PasswordManager.Android.ExportPasswordsProgressBarUsage". Never remove
    // or reuse them, only add new ones if needed (and update PROGRESS_COUNT), to keep past and
    // future UMA reports compatible.
    @VisibleForTesting
    public static final int PROGRESS_NOT_SHOWN = 0;
    @VisibleForTesting
    public static final int PROGRESS_HIDDEN_DIRECTLY = 1;
    @VisibleForTesting
    public static final int PROGRESS_HIDDEN_DELAYED = 2;
    // The number of the other PROGRESS_* constants.
    private static final int PROGRESS_COUNT = 3;

    /**
     * Converts a {@link DialogManager.HideActions} value to a value for the
     * "PasswordManager.Android.ExportPasswordsProgressBarUsage" histogram.
     */
    private int actionToHistogramValue(@DialogManager.HideActions int action) {
        switch (action) {
            case DialogManager.HideActions.NO_OP:
                return PROGRESS_NOT_SHOWN;
            case DialogManager.HideActions.HIDDEN_IMMEDIATELY:
                return PROGRESS_HIDDEN_DIRECTLY;
            case DialogManager.HideActions.HIDING_DELAYED:
                return PROGRESS_HIDDEN_DELAYED;
        }
        // All cases should be covered by the above switch statement.
        assert false;
        return PROGRESS_NOT_SHOWN;
    }

    // Takes care of displaying and hiding the progress bar for exporting, while avoiding
    // flickering.
    private final DialogManager mProgressBarManager = new DialogManager(null);

    /**
     * If an error dialog should be shown, this contains the arguments for it, such as the error
     * message. If no error dialog should be shown, this is null.
     */
    @Nullable
    private ExportErrorDialogFragment.ErrorDialogParams mErrorDialogParams;

    /**
     * Contains the reference to the export warning dialog when it is displayed, so that the dialog
     * can be dismissed if Chrome goes to background (without being killed) and is restored too late
     * for the reauthentication time window to still allow exporting. It is null during all other
     * times.
     */
    @Nullable
    private ExportWarningDialogFragment mExportWarningDialogFragment;

    public DialogManager getDialogManagerForTesting() {
        return mProgressBarManager;
    }

    /** The delegate to provide ExportFlow with essential information from the owning fragment. */
    public interface Delegate {
        /**
         * @return The activity associated with the owning fragment.
         */
        Activity getActivity();

        /**
         * @return The fragment manager associated with the owning fragment.
         */
        FragmentManager getFragmentManager();

        /**
         * @return The ID of the root view of the owning fragment.
         */
        int getViewId();
    }

    /** The concrete delegate instance. It is (re)set in {@link #onCreate}. */
    private Delegate mDelegate;

    /**
     * A hook to be used in the onCreate method of the owning {@link Fragment}. I restores the state
     * of the flow.
     * @param savedInstanceState The {@link Bundle} passed from the fragment's onCreate
     * method.
     * @param delegate The {@link Delegate} for this ExportFlow.
     */
    public void onCreate(Bundle savedInstanceState, Delegate delegate) {
        mDelegate = delegate;

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(SAVED_STATE_EXPORT_STATE)) {
            mExportState = savedInstanceState.getInt(SAVED_STATE_EXPORT_STATE);
            if (mExportState == ExportState.CONFIRMED) {
                // If export is underway, ensure that the UI is updated.
                tryExporting();
            }
        }
        if (savedInstanceState.containsKey(SAVED_STATE_EXPORT_FILE_URI)) {
            String uriString = savedInstanceState.getString(SAVED_STATE_EXPORT_FILE_URI);
            if (uriString.isEmpty()) {
                mExportFileUri = Uri.EMPTY;
            } else {
                mExportFileUri = Uri.parse(uriString);
            }
        }
        if (savedInstanceState.containsKey(SAVED_STATE_ENTRIES_COUNT)) {
            mEntriesCount = savedInstanceState.getInt(SAVED_STATE_ENTRIES_COUNT);
        }
    }

    /**
     * Returns true if the export flow is in progress, i.e., when the user interacts with some of
     * its UI.
     * @return True if in progress, false otherwise.
     */
    public boolean isActive() {
        return mExportState != ExportState.INACTIVE;
    }

    /**
     * A helper method which processes the signal that serialized passwords have been stored in the
     * temporary file. It produces a sharing URI for that file, registers that file for deletion at
     * the shutdown of the Java VM, logs some metrics and continues the flow.
     * @param pathToPasswordsFile The filesystem path to the file containing the serialized
     *                            passwords.
     */
    private void shareSerializedPasswords(String pathToPasswordsFile) {
        // Don't display any UI if the user cancelled the export in the meantime.
        if (mExportState == ExportState.INACTIVE) return;

        File passwordsFile = new File(pathToPasswordsFile);
        passwordsFile.deleteOnExit();

        try {
            mExportFileUri = ContentUriUtils.getContentUriFromFile(passwordsFile);
        } catch (IllegalArgumentException e) {
            showExportErrorAndAbort(R.string.password_settings_export_tips, e.getMessage(),
                    R.string.try_again, HistogramExportResult.WRITE_FAILED);
            return;
        }

        tryExporting();
    }

    /**
     * Returns the path to the directory where serialized passwords are stored.
     * @return A subdirectory of the cache, where serialized passwords are stored.
     */
    @VisibleForTesting
    public static String getTargetDirectory() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ContextUtils.getApplicationContext().getCacheDir() + PASSWORDS_CACHE_DIR;
        }
    }

    /**
     * Starts the password export flow.
     * Current state of export flow: the user just tapped the menu item for export
     * The next steps are: passing reauthentication, confirming the export, waiting for exported
     * data (if needed) and choosing a consumer app for the data.
     */
    public void startExporting() {
        assert mExportState == ExportState.INACTIVE;
        // Disable re-triggering exporting until the current exporting finishes.
        mExportState = ExportState.REQUESTED;

        // Start fetching the serialized passwords now to use the time the user spends
        // reauthenticating and reading the warning message. If the user cancels the export or
        // fails the reauthentication, the serialized passwords will simply get ignored when
        // they arrive.
        mEntriesCount = null;
        PasswordManagerHandlerProvider.getInstance().getPasswordManagerHandler().serializePasswords(
                getTargetDirectory(),
                (int entriesCount, String pathToPasswordsFile)
                        -> {
                    mEntriesCount = entriesCount;
                    shareSerializedPasswords(pathToPasswordsFile);
                },
                (String errorMessage) -> {
                    showExportErrorAndAbort(R.string.password_settings_export_tips, errorMessage,
                            R.string.try_again, HistogramExportResult.WRITE_FAILED);
                });
        if (!ReauthenticationManager.isScreenLockSetUp(
                    mDelegate.getActivity().getApplicationContext())) {
            Toast.makeText(mDelegate.getActivity().getApplicationContext(),
                         R.string.password_export_set_lock_screen, Toast.LENGTH_LONG)
                    .show();
            // Re-enable exporting, the current one was cancelled by Chrome.
            mExportState = ExportState.INACTIVE;
        } else {
            // Always trigger reauthentication at the start of the exporting flow, even if the last
            // one succeeded recently.
            ReauthenticationManager.displayReauthenticationFragment(
                    R.string.lockscreen_description_export, mDelegate.getViewId(),
                    mDelegate.getFragmentManager(), ReauthenticationManager.ReauthScope.BULK);
        }
    }

    /**
     * Continues with the password export flow after the user successfully reauthenticated.
     * Current state of export flow: the user tapped the menu item for export and passed
     * reauthentication. The next steps are: confirming the export, waiting for exported data (if
     * needed) and choosing a consumer app for the data.
     */
    private void exportAfterReauth() {
        assert mExportWarningDialogFragment == null;
        mExportWarningDialogFragment = new ExportWarningDialogFragment();
        mExportWarningDialogFragment.setExportWarningHandler(
                new ExportWarningDialogFragment.Handler() {
                    /**
                     * On positive button response asks the parent to continue with the export flow.
                     */
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        if (which == AlertDialog.BUTTON_POSITIVE) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    PasswordSettings.PASSWORD_EXPORT_EVENT_HISTOGRAM,
                                    PasswordExportEvent.EXPORT_CONFIRMED,
                                    PasswordExportEvent.COUNT);
                            mExportState = ExportState.CONFIRMED;
                            // If the error dialog has been waiting, display it now, otherwise
                            // continue the export flow.
                            if (mErrorDialogParams != null) {
                                showExportErrorDialogFragment();
                            } else {
                                tryExporting();
                            }
                        }
                    }

                    /**
                     * Mark the dismissal of the dialog, so that waiting UI (such as error
                     * reporting) can be shown.
                     */
                    @Override
                    public void onDismiss() {
                        // Unless the positive button action moved the exporting state forward,
                        // cancel the export. This happens both when the user taps the negative
                        // button or when they tap outside of the dialog to dismiss it.
                        if (mExportState != ExportState.CONFIRMED) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    PasswordSettings.PASSWORD_EXPORT_EVENT_HISTOGRAM,
                                    PasswordExportEvent.EXPORT_DISMISSED,
                                    PasswordExportEvent.COUNT);
                            mExportState = ExportState.INACTIVE;
                        }

                        mExportWarningDialogFragment = null;
                        // If the error dialog has been waiting, display it now.
                        if (mErrorDialogParams != null) showExportErrorDialogFragment();
                    }
                });
        mExportWarningDialogFragment.show(mDelegate.getFragmentManager(), null);
    }

    /**
     * Starts the exporting intent if both blocking events are completed: serializing and the
     * confirmation flow.
     * At this point, the user the user has tapped the menu item for export and passed
     * reauthentication. Upon calling this method, the user has either also confirmed the export, or
     * the exported data have been prepared. The method is called twice, once for each of those
     * events. The next step after both the export is confirmed and the data is ready is to offer
     * the user an intent chooser for sharing the exported passwords.
     */
    private void tryExporting() {
        if (mExportState != ExportState.CONFIRMED) return;
        if (mEntriesCount == null) {
            // The serialization has not finished. Until this finishes, a progress bar is
            // displayed with an option to cancel the export.
            ProgressBarDialogFragment progressBarDialogFragment = new ProgressBarDialogFragment();
            progressBarDialogFragment.setCancelProgressHandler(
                    new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            if (which == AlertDialog.BUTTON_NEGATIVE) {
                                mExportState = ExportState.INACTIVE;
                            }
                        }
                    });
            mProgressBarManager.show(progressBarDialogFragment, mDelegate.getFragmentManager());
        } else {
            // Note: if the serialization is quicker than the user interacting with the
            // confirmation dialog, then there is no progress bar shown, in which case hide() is
            // just calling the callback synchronously.
            mProgressBarManager.hide(this::sendExportIntent);
        }
    }

    /**
     * Call this to abort the export UI flow and display an error description to the user.
     * @param descriptionId The resource ID of a string with a brief explanation of the error.
     * @param detailedDescription An optional string with more technical details about the error.
     * @param positiveButtonLabelId The resource ID of the label of the positive button in the error
     * dialog.
     */
    @VisibleForTesting
    public void showExportErrorAndAbort(int descriptionId, @Nullable String detailedDescription,
            int positiveButtonLabelId, @HistogramExportResult int histogramExportResult) {
        assert mErrorDialogParams == null;
        mProgressBarManager.hide(() -> {
            showExportErrorAndAbortImmediately(descriptionId, detailedDescription,
                    positiveButtonLabelId, histogramExportResult);
        });
    }

    public void showExportErrorAndAbortImmediately(int descriptionId,
            @Nullable String detailedDescription, int positiveButtonLabelId,
            @HistogramExportResult int histogramExportResult) {
        mErrorDialogParams = new ExportErrorDialogFragment.ErrorDialogParams();
        mErrorDialogParams.positiveButtonLabelId = positiveButtonLabelId;
        mErrorDialogParams.description =
                mDelegate.getActivity().getResources().getString(descriptionId);

        if (detailedDescription != null) {
            mErrorDialogParams.detailedDescription =
                    mDelegate.getActivity().getResources().getString(
                            R.string.password_settings_export_error_details, detailedDescription);
        }

        if (mExportWarningDialogFragment == null) showExportErrorDialogFragment();
    }

    /**
     * This is a helper method to {@link #showExportErrorAndAbort}, responsible for showing the
     * actual UI.
     */
    private void showExportErrorDialogFragment() {
        assert mErrorDialogParams != null;

        ExportErrorDialogFragment exportErrorDialogFragment = new ExportErrorDialogFragment();
        int positiveButtonLabelId = mErrorDialogParams.positiveButtonLabelId;
        exportErrorDialogFragment.initialize(mErrorDialogParams);
        mErrorDialogParams = null;

        exportErrorDialogFragment.setExportErrorHandler(new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (which == AlertDialog.BUTTON_POSITIVE) {
                    if (positiveButtonLabelId
                            == R.string.password_settings_export_learn_google_drive) {
                        // Link to the help article about how to use Google Drive.
                        Intent intent = new Intent(Intent.ACTION_VIEW,
                                Uri.parse("https://support.google.com/drive/answer/2424384"));
                        intent.setPackage(mDelegate.getActivity().getPackageName());
                        mDelegate.getActivity().startActivity(intent);
                    } else if (positiveButtonLabelId == R.string.try_again) {
                        mExportState = ExportState.REQUESTED;
                        exportAfterReauth();
                    }
                } else if (which == AlertDialog.BUTTON_NEGATIVE) {
                    // Re-enable exporting, the current one was just cancelled.
                    mExportState = ExportState.INACTIVE;
                }
            }
        });
        exportErrorDialogFragment.show(mDelegate.getFragmentManager(), null);
    }

    /**
     * If the URI of the file with exported passwords is not null, passes it into an implicit
     * intent, so that the user can use a storage app to save the exported passwords.
     */
    private void sendExportIntent() {
        assert mExportState == ExportState.CONFIRMED;
        mExportState = ExportState.INACTIVE;

        if (mExportFileUri.equals(Uri.EMPTY)) return;

        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/csv");
        send.putExtra(Intent.EXTRA_STREAM, mExportFileUri);
        send.putExtra(Intent.EXTRA_SUBJECT,
                mDelegate.getActivity().getResources().getString(
                        R.string.password_settings_export_subject));

        try {
            Intent chooser = Intent.createChooser(send, null);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ContextUtils.getApplicationContext().startActivity(chooser);
        } catch (ActivityNotFoundException e) {
            showExportErrorAndAbort(R.string.password_settings_export_no_app, null,
                    R.string.password_settings_export_learn_google_drive,
                    HistogramExportResult.NO_CONSUMER);
        }
        mExportFileUri = null;
    }

    /**
     * A hook to be used in a {@link Fragment}'s onResume method. I processes the result of the
     * reauthentication.
     */
    public void onResume() {
        if (mExportState == ExportState.REQUESTED) {
            // If Chrome returns to foreground from being paused (but without being killed), and
            // exportAfterReauth was called before pausing, the warning dialog is still
            // displayed and ready to be used, and this is indicated by
            // |mExportWarningDialogFragment| being non-null.
            if (ReauthenticationManager.authenticationStillValid(
                        ReauthenticationManager.ReauthScope.BULK)) {
                if (mExportWarningDialogFragment == null) exportAfterReauth();
            } else {
                if (mExportWarningDialogFragment != null) mExportWarningDialogFragment.dismiss();
                mExportState = ExportState.INACTIVE;
            }
        }
    }

    /**
     * A hook to be used in a {@link Fragment}'s onSaveInstanceState method. I saves the state of
     * the flow.
     */
    public void onSaveInstanceState(Bundle outState) {
        outState.putInt(SAVED_STATE_EXPORT_STATE, mExportState);
        if (mEntriesCount != null) {
            outState.putInt(SAVED_STATE_ENTRIES_COUNT, mEntriesCount);
        }
        if (mExportFileUri != null) {
            outState.putString(SAVED_STATE_EXPORT_FILE_URI, mExportFileUri.toString());
        }
    }

    /**
     * Returns whether the password export feature is ready to use.
     * @return Returns true if the Reauthentication Api is available.
     */
    public static boolean providesPasswordExport() {
        return ReauthenticationManager.isReauthenticationApiAvailable();
    }
}
