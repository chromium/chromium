// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.fre;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.ui.R;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.signin.ui.fre.SigninFirstRunCoordinator.Listener;
import org.chromium.chrome.browser.signin.ui.fre.SigninFirstRunProperties.FrePolicy;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

class SigninFirstRunMediator implements AccountsChangeObserver, ProfileDataCache.Observer,
                                        AccountPickerCoordinator.Listener {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Listener mListener;
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private AccountPickerDialogCoordinator mDialogCoordinator;
    private String mSelectedAccountName;

    SigninFirstRunMediator(
            Context context, ModalDialogManager modalDialogManager, Listener listener) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mListener = listener;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel = SigninFirstRunProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked, this::onDismissClicked,
                ExternalAuthUtils.getInstance().canUseGooglePlayServices());

        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
    }

    void onNativeAndPolicyLoaded(boolean hasPolicies) {
        mModel.set(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED, true);
        mModel.set(SigninFirstRunProperties.FRE_POLICY, hasPolicies ? new FrePolicy() : null);
        final boolean isSigninSupported = ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                && !IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .isSigninDisabledByPolicy();
        mModel.set(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED, isSigninSupported);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    @Override
    public void onAccountSelected(String accountName) {
        setSelectedAccountName(accountName);
        if (mDialogCoordinator != null) mDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        mListener.addAccount();
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        if (!mModel.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            mListener.acceptTermsOfService();
            return;
        }
        if (mSelectedAccountName == null) {
            mListener.addAccount();
            return;
        }
        if (mModel.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED)) {
            mListener.acceptTermsOfService();
            return;
        }
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The continue button shouldn't be visible before the native is not initialized!";
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            mListener.acceptTermsOfService();
            return;
        }
        final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        signinManager.onFirstRunCheckDone();
        signinManager.signin(
                AccountUtils.createAccountFromName(mSelectedAccountName), new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mListener.acceptTermsOfService();
                    }

                    @Override
                    public void onSignInAborted() {
                        // TODO(crbug/1248090): Handle the sign-in error here
                    }
                });
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_DISMISS_CLICKED}.
     */
    private void onDismissClicked() {
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The dismiss button shouldn't be visible before the native is not initialized!";
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .signOut(SignoutReason.ABORT_SIGNIN, mListener::acceptTermsOfService,
                            /* forceWipeUserData= */ false);
        } else {
            mListener.acceptTermsOfService();
        }
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountName, accountEmail)) {
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    private void updateAccounts(List<Account> accounts) {
        if (accounts.isEmpty()) {
            mSelectedAccountName = null;
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA, null);
        } else if (mSelectedAccountName == null
                || AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
            setSelectedAccountName(accounts.get(0).name);
        }

        if (accounts.size() == 1) {
            mAccountManagerFacade.checkChildAccountStatus(accounts.get(0), status -> {
                final boolean isChild = ChildAccountStatus.isChild(status);
                mModel.set(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
                if (isChild && mDialogCoordinator != null) {
                    mDialogCoordinator.dismissDialog();
                }
                // Selected account data will be updated in #onProfileDataUpdated()
                mProfileDataCache.setBadge(isChild ? R.drawable.ic_account_child_20dp : 0);
            });
        } else {
            mProfileDataCache.setBadge(0);
            mModel.set(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED, false);
        }
    }
}