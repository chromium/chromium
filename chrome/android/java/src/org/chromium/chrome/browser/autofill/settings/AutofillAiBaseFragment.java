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
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.settings.AutofillAiDelegate.ToggleConfig;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Set;

/** Base class to handle shared logic for Autofill AI leaf settings fragments. */
@NullMarked
public abstract class AutofillAiBaseFragment extends ChromeBaseSettingsFragment
        implements EntityDataManager.EntityDataManagerObserver {

    private @Nullable AutofillAiDelegate mAutofillAiDelegate;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    protected abstract AutofillAiDelegate createAutofillAiDelegate();

    protected abstract Set<Integer> getEntityTypes();

    protected abstract @AutofillOptionsReferrer int getReferrer();

    protected abstract int getTitleResId();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mAutofillAiDelegate = createAutofillAiDelegate();

        mPageTitle.set(getString(getTitleResId()));

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
        if (mAutofillAiDelegate != null) {
            mAutofillAiDelegate.onConfigurationChanged();
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        rebuildEntityList();
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        if (mAutofillAiDelegate != null) {
            mAutofillAiDelegate.onActivityCreated();
        }
    }

    @Override
    public void onDestroyView() {
        if (mAutofillAiDelegate != null) {
            mAutofillAiDelegate.onDestroyView();
        }
        super.onDestroyView();
    }

    @Override
    public void onEntityInstancesChanged() {
        rebuildEntityList();
        notifyPreferencesUpdated();
    }

    protected void rebuildEntityList() {
        PreferenceScreen screen = getPreferenceScreen();
        if (screen == null || mAutofillAiDelegate == null) {
            return;
        }
        screen.removeAll();
        screen.setOrderingAsAdded(true);

        mAutofillAiDelegate.maybeAddDisabledSettingsInfoCard(screen, getReferrer());
        mAutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(screen);
        mAutofillAiDelegate.addAutofillAiEntities(screen, getEntityTypes());
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    static void updateDynamicPreferences(
            ChromeBaseSearchIndexProvider searchIndexProvider,
            SettingsIndexData indexData,
            Profile profile,
            ToggleConfig toggleConfig) {
        String prefFragmentName = searchIndexProvider.getPrefFragmentName();
        AutofillAiDelegate.maybeAddDisabledSettingsInfoCard(indexData, profile, prefFragmentName);
        AutofillAiDelegate.maybeAddDisabledWalletDataSharingDataCard(
                indexData, profile, prefFragmentName);
        AutofillAiDelegate.maybeAddOptInToggle(indexData, prefFragmentName, toggleConfig);
    }
}
