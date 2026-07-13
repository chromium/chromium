// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.personal_context;

import android.content.Context;
import android.os.Bundle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.settings.AutofillHelpMenuProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Fragment for Autofill Personal Context settings. */
@NullMarked
public class AutofillPersonalContextFragment extends ChromeBaseSettingsFragment {
    public static final String PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH =
            "autofill_personal_context_switch";
    public static final String PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS =
            "autofill_personal_context_manage_connected_apps";
    public static final String PREF_PERSONAL_CONTEXT_NOTICE_PREFERENCE =
            "personal_context_notice_preference";

    // User action constants for external entry points
    public static final String ACTION_ENTRY_FROM_IDENTITY_DOCS =
            "PersonalContext.Settings.EntryPoint.IdentityDocsSettings";
    public static final String ACTION_ENTRY_FROM_TRAVEL =
            "PersonalContext.Settings.EntryPoint.TravelSettings";
    public static final String ACTION_ENTRY_FROM_SHOPPING =
            "PersonalContext.Settings.EntryPoint.ShoppingSettings";
    public static final String ACTION_ENTRY_FROM_AUTOFILL_AND_PASSWORDS =
            "PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings";

    // User action constants for internal page actions
    public static final String ACTION_TOGGLED_ON = "PersonalContext.Settings.ToggledOn";
    public static final String ACTION_TOGGLED_OFF = "PersonalContext.Settings.ToggledOff";
    public static final String ACTION_MANAGE_CONNECTED_APPS =
            "PersonalContext.Settings.ManageConnectedAppsClick";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    public AutofillPersonalContextFragment() {}

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        requireActivity()
                .addMenuProvider(new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_personal_context_preferences);
        mPageTitle.set(getString(R.string.personal_context_autofill_settings_title_android));
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Nullable ChromeSwitchPreference getAutofillPersonalContextSwitch() {
        return findPreference(PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH);
    }

    @Nullable Preference getAutofillPersonalContextManageConnectedApps() {
        return findPreference(PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS);
    }

    public static boolean shouldShowPersonalContext(Profile profile) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
                && isPersonalContextEligible(profile);
    }

    public static boolean isPersonalContextEligible(Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)) {
            return false;
        }
        EntityDataManager manager = EntityDataManagerFactory.getForProfile(profile);
        return manager != null && manager.isPersonalContextPreferenceVisible();
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    AutofillPersonalContextFragment.class.getName(),
                    R.xml.autofill_personal_context_preferences) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    if (!shouldShowPersonalContext(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH));
                        indexData.removeEntry(
                                getUniqueId(PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS));
                    }
                }
            };
}
