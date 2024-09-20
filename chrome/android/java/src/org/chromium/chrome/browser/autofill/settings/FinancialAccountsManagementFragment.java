// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

import java.util.Optional;

/** Fragment showing management options for financial accounts like Pix, e-Wallets etc. */
public class FinancialAccountsManagementFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManagerObserver, Preference.OnPreferenceChangeListener {
    private static Callback<Fragment> sObserverForTest;

    // Histograms
    @VisibleForTesting
    static final String FRAGMENT_SHOWN_HISTOGRAM = "FacilitatedPayments.SettingsPage.Shown";

    // TODO(b/337929926): Remove hardcoding for Pix and use  FacilitatedPaymentsType enum.
    @VisibleForTesting
    static final String FACILITATED_PAYMENTS_TOGGLE_UPDATED_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.Pix.ToggleUpdated";

    // Preference keys
    @VisibleForTesting static final String PREFERENCE_KEY_PIX = "pix";
    @VisibleForTesting static final String PREFERENCE_KEY_PIX_BANK_ACCOUNT = "pix_bank_account:%s";

    static final String TITLE_KEY = "financial_accounts_management_title";

    private PersonalDataManager mPersonalDataManager;
    private BankAccount[] mBankAccounts;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        Bundle extras = getArguments();
        String title = "";
        if (extras != null) {
            title = extras.getString(TITLE_KEY, "");
        }
        mPageTitle.set(title);

        setHasOptionsMenu(false);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
        RecordHistogram.recordBooleanHistogram(FRAGMENT_SHOWN_HISTOGRAM, /* sample= */ true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onResume() {
        super.onResume();
        // Rebuild the preference list in case any of the underlying data has been updated and if
        // any preferences need to be added/removed based on that.
        rebuildPage();
    }

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        mBankAccounts = mPersonalDataManager.getMaskedBankAccounts();
        if (mBankAccounts.length == 0) {
            return;
        }

        boolean isFacilitatedPaymentsPixEnabled =
                mPersonalDataManager.getFacilitatedPaymentsPixPref();
        ChromeSwitchPreference pixSwitch = new ChromeSwitchPreference(getStyledContext());
        pixSwitch.setChecked(isFacilitatedPaymentsPixEnabled);
        pixSwitch.setKey(PREFERENCE_KEY_PIX);
        pixSwitch.setTitle(R.string.settings_manage_other_financial_accounts_pix);
        getPreferenceScreen().addPreference(pixSwitch);
        if (isFacilitatedPaymentsPixEnabled) {
            // Show bank accounts only if the Pix switch is enabled.
            addPixAccountPreferences();
        }
        pixSwitch.setOnPreferenceChangeListener(this);
    }

    private void removePixAccountPreferences() {
        for (BankAccount bankAccount : mBankAccounts) {
            Preference bankAccountPref =
                    getPreferenceScreen()
                            .findPreference(
                                    String.format(
                                            PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                                            bankAccount.getInstrumentId()));
            if (bankAccountPref != null) {
                getPreferenceScreen().removePreference(bankAccountPref);
            }
        }
    }

    private void addPixAccountPreferences() {
        for (BankAccount bankAccount : mBankAccounts) {
            getPreferenceScreen().addPreference(getPreferenceForBankAccount(bankAccount));
        }
    }

    private Preference getPreferenceForBankAccount(BankAccount bankAccount) {
        Preference bankAccountPref = new Preference(getStyledContext());

        bankAccountPref.setTitle(bankAccount.getBankName());
        bankAccountPref.setKey(
                String.format(PREFERENCE_KEY_PIX_BANK_ACCOUNT, bankAccount.getInstrumentId()));
        bankAccountPref.setSummary(
                getResources()
                        .getString(
                                R.string.settings_pix_bank_account_identifer,
                                getBankAccountTypeString(bankAccount.getAccountType()),
                                bankAccount.getAccountNumberSuffix()));
        bankAccountPref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
        Optional<Bitmap> displayIconOptional = Optional.empty();
        if (bankAccount.getDisplayIconUrl() != null && bankAccount.getDisplayIconUrl().isValid()) {
            displayIconOptional =
                    mPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(
                            bankAccount.getDisplayIconUrl(),
                            AutofillUiUtils.CardIconSpecs.create(
                                    getStyledContext(), ImageSize.LARGE));
        }
        Drawable displayIconBitmapDrawable =
                displayIconOptional.isPresent()
                        ? new BitmapDrawable(getResources(), displayIconOptional.get())
                        : ResourcesCompat.getDrawable(
                                getResources(),
                                R.drawable.ic_account_balance,
                                getStyledContext().getTheme());
        bankAccountPref.setIcon(displayIconBitmapDrawable);

        return bankAccountPref;
    }

    private String getBankAccountTypeString(@AccountType int bankAccountType) {
        switch (bankAccountType) {
            case AccountType.CHECKING:
                return getResources().getString(R.string.bank_account_type_checking);
            case AccountType.SAVINGS:
                return getResources().getString(R.string.bank_account_type_savings);
            case AccountType.CURRENT:
                return getResources().getString(R.string.bank_account_type_current);
            case AccountType.SALARY:
                return getResources().getString(R.string.bank_account_type_salary);
            case AccountType.TRANSACTING_ACCOUNT:
                return getResources().getString(R.string.bank_account_type_transacting);
            case AccountType.UNKNOWN:
            default:
                return "";
        }
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    // Preference.OnPreferenceChangeListener override.
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (preference.getKey().equals(PREFERENCE_KEY_PIX)) {
            boolean isPixEnabled = (boolean) newValue;
            RecordHistogram.recordBooleanHistogram(
                    FACILITATED_PAYMENTS_TOGGLE_UPDATED_HISTOGRAM, /* sample= */ isPixEnabled);
            mPersonalDataManager.setFacilitatedPaymentsPixPref(isPixEnabled);
            if (isPixEnabled) {
                addPixAccountPreferences();
            } else {
                removePixAccountPreferences();
            }
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
}
