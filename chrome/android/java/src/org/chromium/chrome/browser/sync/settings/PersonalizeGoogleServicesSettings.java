// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.GoogleActivityController;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;

/*
 * Settings fragment containing links to Google Account settings related to Chrome.
 */
public class PersonalizeGoogleServicesSettings extends ChromeBaseSettingsFragment
        implements SyncService.SyncStateChangedListener {
    private static final String PREF_WEB_AND_APP_ACTIVITY = "web_and_app_activity";
    private static final String PREF_LINKED_GOOGLE_SERVICES = "linked_google_services";

    private SyncService mSyncService;
    private Preference mWebAndAppActivity;
    private Preference mLinkedGoogleServices;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        mPageTitle.set(getString(R.string.sign_in_personalize_google_services_title_eea));
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.personalize_google_services_preferences);

        mSyncService = SyncServiceFactory.getForProfile(getProfile());
        mWebAndAppActivity = findPreference(PREF_WEB_AND_APP_ACTIVITY);
        mLinkedGoogleServices = findPreference(PREF_LINKED_GOOGLE_SERVICES);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onStart() {
        super.onStart();
        mSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public void onStop() {
        super.onStop();
        mSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void syncStateChanged() {
        updatePreferences();
    }

    private void updatePreferences() {
        String signedInAccountName =
                CoreAccountInfo.getEmailFrom(
                        IdentityServicesProvider.get()
                                .getIdentityManager(getProfile())
                                .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        // May happen if account is removed from the device while this screen is shown.
        if (signedInAccountName == null) {
            SettingsNavigationFactory.createSettingsNavigation().finishCurrentSettings(this);
            return;
        }

        mWebAndAppActivity.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this, () -> onWebAndAppActivityClicked(signedInAccountName)));
        mLinkedGoogleServices.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(
                        this, () -> onLinkedGoogleServicesClicked(signedInAccountName)));
    }

    private void onWebAndAppActivityClicked(String signedInAccountName) {
        GoogleActivityController.create()
                .openWebAndAppActivitySettings(getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
    }

    private void onLinkedGoogleServicesClicked(String signedInAccountName) {
        GoogleActivityController.create()
                .openLinkedGoogleServicesSettings(getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_LinkedGoogleServicesClicked");
    }
}
