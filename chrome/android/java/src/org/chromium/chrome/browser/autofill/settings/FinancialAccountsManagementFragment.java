// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSize;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.PersonalDataManagerObserver;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

import java.util.Optional;

/** Fragment showing management options for financial accounts like Pix, e-Wallets etc. */
public class FinancialAccountsManagementFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManagerObserver {
    private static Callback<Fragment> sObserverForTest;

    @VisibleForTesting static final String PREFERENCE_KEY_PIX = "pix";
    @VisibleForTesting static final String PREFERENCE_KEY_PIX_BANK_ACCOUNT = "pix_bank_account:%s";

    static final String TITLE_KEY = "financial_accounts_management_title";

    private PersonalDataManager mPersonalDataManager;

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        Bundle extras = getArguments();
        String title = "";
        if (extras != null) {
            title = extras.getString(TITLE_KEY, "");
        }
        getActivity().setTitle(title);
        setHasOptionsMenu(false);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
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

        BankAccount[] bankAccounts = mPersonalDataManager.getMaskedBankAccounts();
        if (bankAccounts.length > 0) {
            ChromeSwitchPreference pixSwitch = new ChromeSwitchPreference(getStyledContext());
            pixSwitch.setKey(PREFERENCE_KEY_PIX);
            pixSwitch.setTitle(R.string.settings_manage_other_financial_accounts_pix);
            getPreferenceScreen().addPreference(pixSwitch);
            for (BankAccount bankAccount : bankAccounts) {
                getPreferenceScreen().addPreference(getPreferenceForBankAccount(bankAccount));
            }
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
        if (bankAccount.getDisplayIconUrl() != null) {
            Optional<Bitmap> displayIconOptional =
                    mPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(
                            bankAccount.getDisplayIconUrl(),
                            CardIconSpecs.create(getStyledContext(), CardIconSize.LARGE));
            if (displayIconOptional.isPresent()) {
                bankAccountPref.setIcon(
                        new BitmapDrawable(getResources(), displayIconOptional.get()));
            }
        }
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
