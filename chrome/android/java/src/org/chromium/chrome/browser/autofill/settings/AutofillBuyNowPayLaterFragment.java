// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/** Preferences fragment to allow users to manage Buy Now Pay Later application settings. */
@NullMarked
public class AutofillBuyNowPayLaterFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver,
                Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    static final String PREF_KEY_ENABLE_BUY_NOW_PAY_LATER = "enable_buy_now_pay_later";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private PersonalDataManager mPersonalDataManager;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.autofill_bnpl_settings_label));

        // Create blank preference screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Rebuild the preference list in case any of the underlying data has been updated.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);
        createBuyNowPayLaterSwitch();
    }

    private void createBuyNowPayLaterSwitch() {
        ChromeSwitchPreference buyNowPayLaterSwitch =
                new ChromeSwitchPreference(getStyledContext());
        buyNowPayLaterSwitch.setTitle(R.string.autofill_bnpl_settings_label);
        buyNowPayLaterSwitch.setSummary(R.string.autofill_bnpl_settings_toggle_sublabel);
        buyNowPayLaterSwitch.setKey(PREF_KEY_ENABLE_BUY_NOW_PAY_LATER);
        buyNowPayLaterSwitch.setChecked(mPersonalDataManager.isBuyNowPayLaterEnabled());
        buyNowPayLaterSwitch.setOnPreferenceChangeListener(this);
        getPreferenceScreen().addPreference(buyNowPayLaterSwitch);
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        mPersonalDataManager.setBuyNowPayLater((boolean) newValue);
        return true;
    }

    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
