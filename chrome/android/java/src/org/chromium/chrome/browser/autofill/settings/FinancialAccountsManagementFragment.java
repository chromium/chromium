// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/** Fragment showing management options for financial accounts like Pix, e-Wallets etc. */
@NullMarked
public class FinancialAccountsManagementFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManagerObserver, Preference.OnPreferenceChangeListener {
    private static @Nullable Callback<Fragment> sObserverForTest;

    // Histograms
    @VisibleForTesting
    static final String FRAGMENT_SHOWN_HISTOGRAM = "FacilitatedPayments.SettingsPage.Shown";

    @VisibleForTesting
    static final String FACILITATED_PAYMENTS_PIX_TOGGLE_UPDATED_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.Pix.ToggleUpdated";

    static final String FACILITATED_PAYMENTS_EWALLET_TOGGLE_UPDATED_HISTOGRAM =
            "FacilitatedPayments.SettingsPage.Ewallet.ToggleUpdated";

    // Preference keys
    @VisibleForTesting static final String PREFERENCE_KEY_PIX = "pix";
    @VisibleForTesting static final String PREFERENCE_KEY_EWALLET = "ewallet";
    @VisibleForTesting static final String PREFERENCE_KEY_PIX_BANK_ACCOUNT = "pix_bank_account:%s";
    @VisibleForTesting static final String PREFERENCE_KEY_EWALLET_ACCOUNT = "ewallet_account:%s";

    static final String TITLE_KEY = "financial_accounts_management_title";

    private PersonalDataManager mPersonalDataManager;
    private Ewallet @Nullable [] mEwallets;
    private BankAccount @Nullable [] mBankAccounts;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private Callback<String> mFinancialAccountManageLinkOpenerCallback =
            url -> CustomTabActivity.showInfoPage(getActivity(), url);

    // ChromeBaseSettingsFragment override.
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
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
        mBankAccounts = mPersonalDataManager.getMaskedBankAccounts();
        if (mEwallets.length == 0 && mBankAccounts.length == 0) {
            return;
        }
        boolean isFacilitatedPaymentsEwalletEnabled =
                mPersonalDataManager.getFacilitatedPaymentsEwalletPref();
        boolean isFacilitatedPaymentsPixEnabled =
                mPersonalDataManager.getFacilitatedPaymentsPixPref();

        if (mEwallets.length > 0) {
            ChromeSwitchPreference eWalletSwitch = new ChromeSwitchPreference(getStyledContext());
            eWalletSwitch.setChecked(isFacilitatedPaymentsEwalletEnabled);
            eWalletSwitch.setKey(PREFERENCE_KEY_EWALLET);
            eWalletSwitch.setTitle(R.string.settings_manage_other_financial_accounts_ewallet);
            getPreferenceScreen().addPreference(eWalletSwitch);
            if (isFacilitatedPaymentsEwalletEnabled) {
                addEwalletRowItems();
            }
            eWalletSwitch.setOnPreferenceChangeListener(this);
        }

        if (mBankAccounts.length > 0) {
            ChromeSwitchPreference pixSwitch = new ChromeSwitchPreference(getStyledContext());
            pixSwitch.setChecked(isFacilitatedPaymentsPixEnabled);
            pixSwitch.setKey(PREFERENCE_KEY_PIX);
            pixSwitch.setTitle(R.string.settings_manage_other_financial_accounts_pix);
            getPreferenceScreen().addPreference(pixSwitch);
            if (isFacilitatedPaymentsPixEnabled) {
                addPixAccountPreferences();
            }
            pixSwitch.setOnPreferenceChangeListener(this);
        }

        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    private void addPixAccountPreferences() {
        for (BankAccount bankAccount : assumeNonNull(mBankAccounts)) {
            getPreferenceScreen().addPreference(getPreferenceForBankAccount(bankAccount));
        }
    }

    private void addEwalletRowItems() {
        for (Ewallet eWallet : assumeNonNull(mEwallets)) {
            getPreferenceScreen().addPreference(getEwalletRowItem(eWallet));
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
        bankAccountPref.setIcon(
                AutofillImageFetcherFactory.getForProfile(getProfile())
                        .getPixAccountIcon(getStyledContext(), bankAccount.getDisplayIconUrl()));
        bankAccountPref.setOnPreferenceClickListener(
                preference -> {
                    mFinancialAccountManageLinkOpenerCallback.onResult(
                            AutofillUiUtils.getManagePaymentMethodUrlForInstrumentId(
                                    bankAccount.getInstrumentId()));
                    return true;
                });

        return bankAccountPref;
    }

    private Preference getEwalletRowItem(Ewallet eWallet) {
        Preference eWalletPref = new Preference(getStyledContext());

        eWalletPref.setTitle(eWallet.getEwalletName());
        eWalletPref.setKey(
                String.format(PREFERENCE_KEY_EWALLET_ACCOUNT, eWallet.getInstrumentId()));
        eWalletPref.setSummary(
                getResources()
                        .getString(
                                R.string.settings_ewallet_account_identifer,
                                eWallet.getAccountDisplayName()));
        eWalletPref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
        eWalletPref.setIcon(
                AutofillUiUtils.getCardIcon(
                        getStyledContext(),
                        AutofillImageFetcherFactory.getForProfile(getProfile()),
                        eWallet.getDisplayIconUrl(),
                        R.drawable.ic_account_balance,
                        ImageSize.LARGE,
                        /* showCustomIcon= */ true));

        eWalletPref.setOnPreferenceClickListener(
                preference -> {
                    mFinancialAccountManageLinkOpenerCallback.onResult(
                            AutofillUiUtils.getManagePaymentMethodUrlForInstrumentId(
                                    eWallet.getInstrumentId()));
                    return true;
                });

        return eWalletPref;
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
                    FACILITATED_PAYMENTS_PIX_TOGGLE_UPDATED_HISTOGRAM, /* sample= */ isPixEnabled);
            mPersonalDataManager.setFacilitatedPaymentsPixPref(isPixEnabled);
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::rebuildPage);
            return true;
        } else if (preference.getKey().equals(PREFERENCE_KEY_EWALLET)) {
            boolean isEwalletEnabled = (boolean) newValue;
            RecordHistogram.recordBooleanHistogram(
                    FACILITATED_PAYMENTS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
                    /* sample= */ isEwalletEnabled);
            mPersonalDataManager.setFacilitatedPaymentsEwalletPref(isEwalletEnabled);
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

    public void setFinancialAccountManageLinkOpenerCallbackForTesting(Callback<String> callback) {
        mFinancialAccountManageLinkOpenerCallback = callback;
    }

    @VisibleForTesting
    static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
