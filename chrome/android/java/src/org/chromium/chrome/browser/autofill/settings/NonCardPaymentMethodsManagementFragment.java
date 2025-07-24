// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/** Fragment showing management options for non-card payment accounts like e-Wallets etc. */
@NullMarked
public class NonCardPaymentMethodsManagementFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManagerObserver, Preference.OnPreferenceChangeListener {
    private static @Nullable Callback<Fragment> sObserverForTest;

    // Histograms
    @VisibleForTesting
    static final String FRAGMENT_SHOWN_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.NonCardPaymentMethodsManagement.Shown";

    @VisibleForTesting
    static final String NON_CARD_PAYMENT_METHODS_EWALLET_TOGGLE_UPDATED_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.NonCardPaymentMethodsManagement.Ewallet.ToggleUpdated";

    @VisibleForTesting
    static final String NON_CARD_PAYMENT_METHODS_A2A_TOGGLE_UPDATED_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.NonCardPaymentMethodsManagement.A2A.ToggleUpdated";

    // Preference keys
    @VisibleForTesting static final String PREFERENCE_KEY_EWALLET = "ewallet";
    @VisibleForTesting static final String PREFERENCE_KEY_EWALLET_ACCOUNT = "ewallet_account:%s";
    @VisibleForTesting static final String PREFERENCE_KEY_A2A = "a2a";
    private final Callback<String> mFinancialAccountManageLinkOpenerCallback =
            url -> CustomTabActivity.showInfoPage(getActivity(), url);
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private PersonalDataManager mPersonalDataManager;
    private Ewallet @Nullable [] mEwallets;

    // ChromeBaseSettingsFragment override.
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(
                getResources().getString(R.string.settings_manage_non_card_payment_methods_title));
        setHasOptionsMenu(false);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);
        setPreferenceScreen(screen);
        RecordHistogram.recordBooleanHistogram(FRAGMENT_SHOWN_HISTOGRAM, /* sample= */ true);
    }

    // EmbeddableSettingsPage implementation.
    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    // ChromeBaseSettingsFragment override.
    @Override
    public void onStart() {
        super.onStart();
        // Rebuild the preference list in case any of the underlying data has been updated and if
        // any preferences need to be added/removed based on that.
        rebuildPage();
    }

    // ChromeBaseSettingsFragment override.
    @Override
    public void onActivityCreated(@Nullable Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    // ChromeBaseSettingsFragment override.
    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);
        mEwallets = mPersonalDataManager.getEwallets();
        if (mEwallets.length > 0) {
            boolean isFacilitatedPaymentsEwalletEnabled =
                    mPersonalDataManager.getFacilitatedPaymentsEwalletPref();
            ChromeSwitchPreference ewalletSwitch = new ChromeSwitchPreference(getStyledContext());
            ewalletSwitch.setChecked(isFacilitatedPaymentsEwalletEnabled);
            ewalletSwitch.setKey(PREFERENCE_KEY_EWALLET);
            ewalletSwitch.setTitle(R.string.settings_manage_other_financial_accounts_ewallet);
            ewalletSwitch.setOnPreferenceChangeListener(this);
            getPreferenceScreen().addPreference(ewalletSwitch);
            if (isFacilitatedPaymentsEwalletEnabled) {
                for (Ewallet ewallet : mEwallets) {
                    getPreferenceScreen().addPreference(getEwalletRowItem(ewallet));
                }
            }
        }
        if (mPersonalDataManager.getFacilitatedPaymentsA2ATriggeredOncePref()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT)) {
            ChromeSwitchPreference a2aSwitch = new ChromeSwitchPreference(getStyledContext());
            a2aSwitch.setChecked(mPersonalDataManager.getFacilitatedPaymentsA2AEnabledPref());
            a2aSwitch.setKey(PREFERENCE_KEY_A2A);
            a2aSwitch.setTitle(R.string.settings_manage_non_card_payment_methods_a2a_title);
            a2aSwitch.setSummary(R.string.settings_manage_non_card_payment_methods_a2a_description);
            a2aSwitch.setOnPreferenceChangeListener(this);
            getPreferenceScreen().addPreference(a2aSwitch);
        }
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    private Preference getEwalletRowItem(Ewallet ewallet) {
        Preference ewalletPref = new Preference(getStyledContext());
        ewalletPref.setTitle(ewallet.getEwalletName());
        ewalletPref.setKey(
                String.format(PREFERENCE_KEY_EWALLET_ACCOUNT, ewallet.getInstrumentId()));
        ewalletPref.setSummary(
                getResources()
                        .getString(
                                R.string.settings_ewallet_account_identifer,
                                ewallet.getAccountDisplayName()));
        ewalletPref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
        ewalletPref.setIcon(
                AutofillUiUtils.getCardIcon(
                        getStyledContext(),
                        AutofillImageFetcherFactory.getForProfile(getProfile()),
                        ewallet.getDisplayIconUrl(),
                        R.drawable.ic_account_balance,
                        ImageSize.LARGE,
                        /* showCustomIcon= */ true));
        ewalletPref.setOnPreferenceClickListener(
                preference -> {
                    mFinancialAccountManageLinkOpenerCallback.onResult(
                            AutofillUiUtils.getManagePaymentMethodUrlForInstrumentId(
                                    ewallet.getInstrumentId()));
                    return true;
                });
        return ewalletPref;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    // Preference.OnPreferenceChangeListener override.
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference.getKey().equals(PREFERENCE_KEY_EWALLET)) {
            boolean isEwalletEnabled = (boolean) newValue;
            RecordHistogram.recordBooleanHistogram(
                    NON_CARD_PAYMENT_METHODS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
                    /* sample= */ isEwalletEnabled);
            mPersonalDataManager.setFacilitatedPaymentsEwalletPref(isEwalletEnabled);
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::rebuildPage);
            return true;
        } else if (preference.getKey().equals(PREFERENCE_KEY_A2A)) {
            boolean isA2aEnabled = (boolean) newValue;
            RecordHistogram.recordBooleanHistogram(
                    NON_CARD_PAYMENT_METHODS_A2A_TOGGLE_UPDATED_HISTOGRAM,
                    /* sample= */ isA2aEnabled);
            mPersonalDataManager.setFacilitatedPaymentsA2AEnabledPref(isA2aEnabled);
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::rebuildPage);
            return true;
        }
        return false;
    }

    // PersonalDataManagerObserver implementation.
    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @VisibleForTesting
    static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    // ChromeBaseSettingsFragment implementation.
    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
