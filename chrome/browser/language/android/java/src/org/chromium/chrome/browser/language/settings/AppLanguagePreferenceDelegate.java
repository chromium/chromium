// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

/**
 * Helper class to manage the preferences UI when selecting an app language from LanguageSettings.
 * This helper is responsible for starting the language split download, showing a Snackbar when
 * the download completes, and updating the summary text on the {@link LanguageItemPikerPreference}
 * representing the overridden app language.
 */
public class AppLanguagePreferenceDelegate {
    /**
     * Interface for holding the Chrome restart action. Passed in from {@link SettingsActivity}.
     */
    public interface RestartAction {
        void restart();
    }

    private SnackbarManager mSnackbarManager;
    private Snackbar mSnackbar;
    private SnackbarController mStackbarController;
    // Preference representing the current app language.
    private LanguageItemPickerPreference mPreference;
    // Activity representing the {@link LanguageSettings} preferences.
    private Activity mActivity;

    /**
     * Set the restart action. This action handler is passed in from {@link SettingsActivity} when
     * the {@link LanguageSettings} fragment is created.
     * @param action RestartAction handler to restart Chrome from the Snackbar.
     */
    public void setRestartAction(RestartAction action) {
        mStackbarController = new SuccessSnackbarControllerImpl(action);
    }

    /**
     * Set the LanguageSettings PreferenceFragment that is currently active and the preference for
     * the app language. Creates a {@link SnackbarManager} using the fragments activity.
     * @param fragment LanguageSettings PreferenceFragment.
     * @param preference LanguageItemPickerPreference for the app language.
     */
    public void setup(LanguageSettings fragment, LanguageItemPickerPreference preference) {
        mActivity = fragment.getActivity();
        mPreference = preference;
        mSnackbarManager =
                new SnackbarManager(mActivity, mActivity.findViewById(android.R.id.content), null);
    }

    /**
     * Show the {@link Snackbar} if one can be shown and there is a saved Snackbar to show.
     */
    public void maybeShowSnackbar() {
        if (mSnackbar != null && mSnackbarManager.canShowSnackbar()) {
            mSnackbarManager.showSnackbar(mSnackbar);
            mSnackbar = null;
        }
    }

    /**
     * Start the download and installation of a language split for the BCP-47 formatted language
     * |code| and register a listener when the installation is complete or has failed. The App
     * Language preference is disabled while the download is occurring to prevent simultaneous
     * language downloads.
     * @param code String language code to be downloaded and installed.
     */
    public void startLanguageSplitDownload(String code) {
        assert mActivity != null : "mActivity must be set to start language split download";
        assert mPreference != null : "mPreference must be set to start langauge split download";
        // Set language text and initial downloading summary.
        mPreference.setLanguageItem(code);
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence downloadingMessage =
                mActivity.getResources().getString(R.string.languages_split_downloading);
        // TODO (https://crbug.com/1197364) Add placeholder to string.
        CharSequence summary = TextUtils.concat(nativeName, " - ", downloadingMessage);
        mPreference.setSummary(summary);

        // Disable preference so a second downloaded cannot be started while one is in progress.
        mPreference.setEnabled(false);

        AppLocaleUtils.setAppLanguagePref(code, (success) -> {
            if (success) {
                languageSplitDownloadComplete();
            } else {
                languageSplitDownloadFailed();
            }
        });
    }

    /**
     * Callback to update the UI when a language split has successfully been installed.
     */
    private void languageSplitDownloadComplete() {
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence appName = BuildInfo.getInstance().hostPackageLabel;
        CharSequence downloadReadyMessage =
                mActivity.getResources().getString(R.string.languages_split_ready, appName);
        // TODO (https://crbug.com/1197364) Add placeholder to string.
        CharSequence summary = TextUtils.concat(nativeName, " - ", downloadReadyMessage);
        mPreference.setSummary(summary);
        mPreference.setEnabled(true);

        makeAndShowRestartSnackbar();
        // TODO (https://crbug.com/1196144): Add logging.
    }

    /**
     * Callback to update the UI when a language split installation has failed.
     */
    private void languageSplitDownloadFailed() {
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence downloadFailedMessage = mActivity.getResources().getString(
                R.string.download_failed_reason_unknown_error, "");
        CharSequence summary = TextUtils.concat(nativeName, " - ", downloadFailedMessage);
        mPreference.setSummary(summary);
        mPreference.setEnabled(true);

        // TODO (https://crbug.com/1196144): Add logging.
    }

    /**
     * Make the restart {@link Snackbar} after a successfully downloaded language split. Try to show
     * the Snackbar, and if not possible save the Snackbar to show later.
     */
    private void makeAndShowRestartSnackbar() {
        mSnackbarManager.dismissSnackbars(mStackbarController);
        String displayName = mPreference.getLanguageItem().getDisplayName();
        Resources resources = mActivity.getResources();
        Snackbar snackbar =
                Snackbar.make(resources.getString(R.string.languages_infobar_ready, displayName),
                                mStackbarController, Snackbar.TYPE_PERSISTENT,
                                Snackbar.UMA_TAB_CLOSE_UNDO)
                        .setAction(resources.getString(R.string.languages_infobar_restart), null);
        if (mSnackbarManager.canShowSnackbar()) {
            mSnackbarManager.showSnackbar(snackbar);
        } else {
            mSnackbar = snackbar;
        }
    }

    // Inner class for successfully downloaded language split SnackbarController.
    private static class SuccessSnackbarControllerImpl implements SnackbarController {
        private RestartAction mRestartAction;

        SuccessSnackbarControllerImpl(RestartAction action) {
            mRestartAction = action;
        }

        // SnackbarController implementation.
        @Override
        public void onAction(Object actionData) {
            // TODO (https://crbug.com/1196144): Add logging.
            if (mRestartAction != null) mRestartAction.restart();
        }

        /**
         * Override SnackbarManager.SnackbarController onDismissNoAction.
         */
        @Override
        public void onDismissNoAction(Object actionData) {
            // TODO (https://crbug.com/1196144): Add logging.
        }
    }
}
