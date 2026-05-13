// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment.AutofillOptionsReferrer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Set;

/** Fragment to manage Autofill AI Travel information. */
@NullMarked
public class AutofillTravelFragment extends ChromeBaseSettingsFragment
        implements EntityDataManager.EntityDataManagerObserver {

    public static final String PREF_OPT_IN_TOGGLE = "autofill_ai_travel_opt_in";

    private static final Set<Integer> TRAVEL_TYPES =
            Set.of(
                    EntityTypeName.FLIGHT_RESERVATION,
                    EntityTypeName.KNOWN_TRAVELER_NUMBER,
                    EntityTypeName.REDRESS_NUMBER,
                    EntityTypeName.VEHICLE);

    private final AutofillAiDelegate mAutofillAiDelegate = new AutofillAiDelegate(this, this);

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_travel_title));

        requireActivity()
                .addMenuProvider(new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);

        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mAutofillAiDelegate.onConfigurationChanged();
    }

    @Override
    public void onStart() {
        super.onStart();
        rebuildEntityList();
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mAutofillAiDelegate.onActivityCreated();
    }

    @Override
    public void onDestroyView() {
        mAutofillAiDelegate.onDestroyView();
        super.onDestroyView();
    }

    @Override
    public void onEntityInstancesChanged() {
        rebuildEntityList();
        notifyPreferencesUpdated();
    }

    private void rebuildEntityList() {
        PreferenceScreen screen = getPreferenceScreen();
        if (screen == null) {
            return;
        }
        screen.removeAll();
        screen.setOrderingAsAdded(true);

        mAutofillAiDelegate.maybeAddDisabledSettingsInfoCard(
                screen, AutofillOptionsReferrer.AUTOFILL_TRAVEL_FRAGMENT);
        mAutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(screen);
        if (shouldShowOptInToggle()) {
            addOptInToggle(screen);
        }

        mAutofillAiDelegate.addAutofillAiEntities(screen, TRAVEL_TYPES);
    }

    private void addOptInToggle(PreferenceScreen screen) {
        // TODO(crbug.com/482994258): Toggle visibility and state handling.

        ChromeSwitchPreference optInToggle = new ChromeSwitchPreference(getStyledContext());
        optInToggle.setKey(PREF_OPT_IN_TOGGLE);
        optInToggle.setTitle(R.string.autofill_travel_opt_in_toggle_label);
        optInToggle.setSummary(R.string.autofill_travel_opt_in_toggle_sub_label);
        screen.addPreference(optInToggle);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    private static boolean shouldShowOptInToggle() {
        // TODO(crbug.com/482994258): Implement proper visibility logic for Travel.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(AutofillTravelFragment.class.getName(), 0) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    AutofillAiDelegate.maybeAddDisabledSettingsInfoCard(
                            indexData, profile, getPrefFragmentName());
                    AutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(
                            indexData, profile, getPrefFragmentName());

                    if (shouldShowOptInToggle()) {
                        indexData.addEntryForKey(
                                getPrefFragmentName(),
                                PREF_OPT_IN_TOGGLE,
                                R.string.autofill_travel_opt_in_toggle_label,
                                R.string.autofill_travel_opt_in_toggle_sub_label);
                    }
                }
            };
}
