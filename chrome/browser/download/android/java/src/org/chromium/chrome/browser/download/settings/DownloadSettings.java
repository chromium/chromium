// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.MimeUtils;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment containing Download settings. */
@NullMarked
public class DownloadSettings extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String PREF_LOCATION_CHANGE = "location_change";
    public static final String PREF_LOCATION_PROMPT_ENABLED = "location_prompt_enabled";
    public static final String PREF_AUTO_OPEN_PDF_ENABLED = "auto_open_pdf_enabled";

    private DownloadLocationPreference mLocationChangePref;
    private ChromeSwitchPreference mLocationPromptEnabledPref;
    private ManagedPreferenceDelegate mLocationPromptEnabledPrefDelegate;
    private ChromeSwitchPreference mAutoOpenPdfEnabledPref;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String s) {
        mPageTitle.set(getString(R.string.menu_downloads));
        SettingsUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        mLocationPromptEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_LOCATION_PROMPT_ENABLED);
        mLocationPromptEnabledPrefDelegate =
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return DownloadDialogBridge.isLocationDialogManaged(getProfile());
                    }
                };
        mLocationPromptEnabledPref.setManagedPreferenceDelegate(mLocationPromptEnabledPrefDelegate);
        if (shouldHideLocationPromptPref(getProfile())) {
            mLocationPromptEnabledPref.setVisible(false);
        } else {
            mLocationPromptEnabledPref.setOnPreferenceChangeListener(this);
        }

        mLocationChangePref = (DownloadLocationPreference) findPreference(PREF_LOCATION_CHANGE);
        mLocationChangePref.setDownloadLocationHelper(new DownloadLocationHelperImpl(getProfile()));

        mAutoOpenPdfEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_AUTO_OPEN_PDF_ENABLED);
        if (shouldEnableAutoOpenPdf(getProfile())) {
            mAutoOpenPdfEnabledPref.setVisible(false);
        } else {
            mAutoOpenPdfEnabledPref.setOnPreferenceChangeListener(this);
            String summary =
                    (MimeUtils.getPdfIntentHandlers().size() == 1)
                            ? getActivity()
                                    .getString(
                                            R.string.auto_open_pdf_enabled_with_app_description,
                                            MimeUtils.getDefaultPdfViewerName())
                            : getActivity().getString(R.string.auto_open_pdf_enabled_description);
            mAutoOpenPdfEnabledPref.setSummaryOn(summary);
        }
    }

    private static boolean shouldHideLocationPromptPref(Profile profile) {
        if (isSystemFileDialogEnabled()) {
            return false;
        }
        return PdfUtils.shouldOpenPdfInline(profile.isOffTheRecord())
                && DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories().isEmpty();
    }

    private static boolean isSystemFileDialogEnabled() {
        return DeviceInfo.isDesktop()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ENABLE_DOWNLOAD_SAVE_AS_SYSTEM_FILE_DIALOG);
    }

    private static boolean shouldEnableAutoOpenPdf(Profile profile) {
        return PdfUtils.shouldOpenPdfInline(profile.isOffTheRecord());
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof DownloadLocationPreference) {
            DownloadLocationPreferenceDialog dialogFragment =
                    DownloadLocationPreferenceDialog.newInstance(
                            (DownloadLocationPreference) preference);
            dialogFragment.setTargetFragment(this, 0);
            dialogFragment.show(getParentFragmentManager(), DownloadLocationPreferenceDialog.TAG);
        } else {
            super.onDisplayPreferenceDialog(preference);
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        updateDownloadSettings();
    }

    private void updateDownloadSettings() {
        mLocationChangePref.updateSummary();

        boolean isLocationDialogManaged =
                DownloadDialogBridge.isLocationDialogManaged(getProfile());
        if (isLocationDialogManaged || isSystemFileDialogEnabled()) {
            // Location prompt can be controlled by enterprise policy or synced pref when OS file
            // picker is enabled.
            mLocationPromptEnabledPref.setChecked(
                    DownloadDialogBridge.getPromptForDownload(getProfile()));
            if (!isLocationDialogManaged) {
                mLocationPromptEnabledPref.setEnabled(true);
            }
        } else {
            // Location prompt is marked enabled if the prompt status is not DONT_SHOW.
            boolean isLocationPromptEnabled =
                    DownloadDialogBridge.getPromptForDownloadAndroid(getProfile())
                            != DownloadPromptStatus.DONT_SHOW;
            mLocationPromptEnabledPref.setChecked(isLocationPromptEnabled);
            mLocationPromptEnabledPref.setEnabled(true);
        }
        if (!PdfUtils.shouldOpenPdfInline(getProfile().isOffTheRecord())) {
            mAutoOpenPdfEnabledPref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.AUTO_OPEN_PDF_ENABLED));
            mAutoOpenPdfEnabledPref.setEnabled(true);
        }
    }

    // Preference.OnPreferenceChangeListener implementation.
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_LOCATION_PROMPT_ENABLED.equals(preference.getKey())) {
            if (isSystemFileDialogEnabled()) {
                DownloadDialogBridge.setPromptForDownload(getProfile(), (boolean) newValue);
            } else if ((boolean) newValue) {
                // Only update if the download location dialog has been shown before.
                if (DownloadDialogBridge.getPromptForDownloadAndroid(getProfile())
                        != DownloadPromptStatus.SHOW_INITIAL) {
                    DownloadDialogBridge.setPromptForDownloadAndroid(
                            getProfile(), DownloadPromptStatus.SHOW_PREFERENCE);
                }
            } else {
                DownloadDialogBridge.setPromptForDownloadAndroid(
                        getProfile(), DownloadPromptStatus.DONT_SHOW);
            }
        } else if (PREF_AUTO_OPEN_PDF_ENABLED.equals(preference.getKey())) {
            UserPrefs.get(getProfile()).setBoolean(Pref.AUTO_OPEN_PDF_ENABLED, (boolean) newValue);
        }
        return true;
    }

    public ManagedPreferenceDelegate getLocationPromptEnabledPrefDelegateForTesting() {
        return mLocationPromptEnabledPrefDelegate;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "downloads";
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    DownloadSettings.class.getName(), R.xml.download_preferences) {

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    if (shouldHideLocationPromptPref(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_LOCATION_PROMPT_ENABLED));
                    }
                    if (shouldEnableAutoOpenPdf(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_AUTO_OPEN_PDF_ENABLED));
                    }
                }
            };
}
