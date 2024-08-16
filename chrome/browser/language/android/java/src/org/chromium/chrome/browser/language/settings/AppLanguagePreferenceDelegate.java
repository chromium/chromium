// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.res.Resources;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.language.AppLocaleUtils;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.ui.util.TokenHolder;

/**
 * Helper class to manage the preferences UI when selecting an app language from LanguageSettings.
 * This helper is responsible for starting the language split download, showing a Snackbar when the
 * download completes, and updating the summary text on the {@link LanguageItemPikerPreference}
 * representing the overridden app language.
 */
public class AppLanguagePreferenceDelegate {
    /** Interface for holding the Chrome restart action. Passed in from {@link SettingsActivity}. */
    public interface RestartAction {
        void restart();
    }

    private int mSnackbarToken = TokenHolder.INVALID_TOKEN;
    private SnackbarManager mSnackbarManager;
    private Snackbar mSnackbar;
    private SnackbarController mSnackbarController;
    // Preference representing the current app language.
    private LanguageItemPickerPreference mPreference;
    // Activity representing the {@link LanguageSettings} preferences.
    private Activity mActivity;
    private Profile mProfile;

    /**
     * Set the restart action. This action handler is passed in from {@link SettingsActivity} when
     * the {@link LanguageSettings} fragment is created.
     *
     * @param action RestartAction handler to restart Chrome from the Snackbar.
     */
    public void setRestartAction(RestartAction action) {
        mSnackbarController = new SuccessSnackbarControllerImpl(action);
    }

    /**
     * Set the LanguageSettings PreferenceFragment that is currently active and the preference for
     * the app language. Creates a {@link SnackbarManager} using the fragments activity.
     *
     * @param fragment LanguageSettings PreferenceFragment.
     * @param preference LanguageItemPickerPreference for the app language.
     * @param profile The Profile for the current session.
     */
    public void setup(
            LanguageSettings fragment, LanguageItemPickerPreference preference, Profile profile) {
        mActivity = fragment.getActivity();
        mPreference = preference;
        mSnackbarManager =
                new SnackbarManager(mActivity, mActivity.findViewById(android.R.id.content), null);
        mProfile = profile;
    }

    /** Show the {@link Snackbar} if one can be shown and there is a saved Snackbar to show. */
    public void maybeShowSnackbar() {
        if (mSnackbar != null && mSnackbarManager.canShowSnackbar()) {
            if (mSnackbarToken == TokenHolder.INVALID_TOKEN) {
                // SnackbarManager is created/owned by this class, so the override doesn't need to
                // be popped.
                mSnackbarToken =
                        mSnackbarManager.pushParentViewToOverrideStack(
                                mActivity.findViewById(android.R.id.content));
            }
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
        assert mPreference != null : "mPreference must be set to start language split download";
        // Set language text and initial downloading summary.
        mPreference.setLanguageItem(mProfile, code);
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence summary =
                mActivity
                        .getResources()
                        .getString(R.string.languages_split_downloading, nativeName);
        mPreference.setSummary(summary);

        // Disable preference so a second downloaded cannot be started while one is in progress.
        mPreference.setEnabled(false);

        AppLocaleUtils.setAppLanguagePref(
                code,
                (success) -> {
                    if (success) {
                        languageSplitDownloadComplete();
                    } else {
                        languageSplitDownloadFailed();
                    }
                });
    }

    /** Callback to update the UI when a language split has successfully been installed. */
    private void languageSplitDownloadComplete() {
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence appName = BuildInfo.getInstance().hostPackageLabel;
        CharSequence summary =
                mActivity
                        .getResources()
                        .getString(R.string.languages_split_ready, nativeName, appName);
        mPreference.setSummary(summary);
        mPreference.setEnabled(true);

        makeAndShowRestartSnackbar();
    }

    /** Callback to update the UI when a language split installation has failed. */
    private void languageSplitDownloadFailed() {
        CharSequence nativeName = mPreference.getLanguageItem().getNativeDisplayName();
        CharSequence summary =
                mActivity.getResources().getString(R.string.languages_split_failed, nativeName);
        mPreference.setSummary(summary);
        mPreference.setEnabled(true);
    }

    /**
     * Make the restart {@link Snackbar} after a successfully downloaded language split. Try to show
     * the Snackbar, and if not possible save the Snackbar to show later.
     */
    private void makeAndShowRestartSnackbar() {
        mSnackbarManager.dismissSnackbars(mSnackbarController);
        String displayName = mPreference.getLanguageItem().getDisplayName();
        Resources resources = mActivity.getResources();
        Snackbar snackbar =
                Snackbar.make(
                                resources.getString(R.string.languages_infobar_ready, displayName),
                                mSnackbarController,
                                Snackbar.TYPE_PERSISTENT,
                                Snackbar.UMA_LANGUAGE_SPLIT_RESTART)
                        .setAction(resources.getString(R.string.languages_infobar_restart), null);
        snackbar.setSingleLine(false);
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

        @Override
        public void onAction(Object actionData) {
            if (mRestartAction != null) mRestartAction.restart();
        }
    }
}
