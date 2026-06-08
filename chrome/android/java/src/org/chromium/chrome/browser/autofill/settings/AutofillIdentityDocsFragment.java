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
import org.chromium.chrome.browser.autofill.settings.AutofillAiDelegate.ToggleConfig;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Set;

/** Fragment to manage Autofill AI Identity Documents. */
@NullMarked
public class AutofillIdentityDocsFragment extends ChromeBaseSettingsFragment
        implements EntityDataManager.EntityDataManagerObserver {

    public static final String PREF_OPT_IN_TOGGLE = "autofill_ai_identity_docs_opt_in";

    private static final ToggleConfig TOGGLE_CONFIG_IDENTITY = new ToggleConfig(PREF_OPT_IN_TOGGLE,
            R.string.autofill_identity_docs_opt_in_toggle_label,
            R.string.autofill_identity_docs_opt_in_toggle_sub_label,
            Pref.AUTOFILL_AI_IDENTITY_ENTITIES_ENABLED);

    private static final Set<Integer> IDENTITY_DOC_TYPES =
            Set.of(
                    EntityTypeName.PASSPORT,
                    EntityTypeName.DRIVERS_LICENSE,
                    EntityTypeName.NATIONAL_ID_CARD);

    private final AutofillAiDelegate mAutofillAiDelegate = new AutofillAiDelegate(this, this, TOGGLE_CONFIG_IDENTITY);

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_identity_docs_title));

        requireActivity()
                .addMenuProvider(new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);

        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);
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
                screen, AutofillOptionsReferrer.AUTOFILL_IDENTITY_DOCS_FRAGMENT);
        mAutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(screen);
        mAutofillAiDelegate.addAutofillAiEntities(screen, IDENTITY_DOC_TYPES);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(AutofillIdentityDocsFragment.class.getName(), 0) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    AutofillAiDelegate.maybeAddDisabledSettingsInfoCard(
                            indexData, profile, getPrefFragmentName());
                    AutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(
                            indexData, profile, getPrefFragmentName());
                    AutofillAiDelegate.maybeAddOptInToggle(indexData, getPrefFragmentName(),
                        TOGGLE_CONFIG_IDENTITY);
                }
            };
}
