// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerBottomSheetProperties.ViewState;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator of the account picker bottom sheet in web sign-in flow.
 */
class AccountPickerBottomSheetMediator implements AccountPickerCoordinator.Listener,
                                                  AccountPickerBottomSheetView.BackPressListener {
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final ProfileDataCache mProfileDataCache;
    private final PropertyModel mModel;

    private final ProfileDataCache.Observer mProfileDataSourceObserver =
            this::updateSelectedAccountData;
    private final AccountManagerFacade mAccountManagerFacade;
    private final AccountsChangeObserver mAccountsChangeObserver = this::onAccountListUpdated;
    private @Nullable String mSelectedAccountName;
    private @Nullable String mDefaultAccountName;
    private @Nullable String mAddedAccountName;

    AccountPickerBottomSheetMediator(Context context, AccountPickerDelegate accountPickerDelegate,
            Runnable dismissBottomSheetRunnable) {
        mAccountPickerDelegate = accountPickerDelegate;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);

        OnClickListener onDismissClicked = v -> dismissBottomSheetRunnable.run();

        mModel = AccountPickerBottomSheetProperties.createModel(
                this::onSelectedAccountClicked, this::onContinueAsClicked, onDismissClicked);
        mProfileDataCache.addObserver(mProfileDataSourceObserver);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(mAccountsChangeObserver);
        mAddedAccountName = null;
        onAccountListUpdated();
    }

    /**
     * Notifies that the user has selected an account.
     *
     * @param accountName The email of the selected account.
     * @param isDefaultAccount Whether the selected account is the first in the account list.
     *
     * TODO(https://crbug.com/1115965): Use CoreAccountInfo instead of account's email
     * as the first argument of the method.
     */
    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        // Clicking on one account in the account list when the account list is expanded
        // will collapse it to the selected account
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.COLLAPSED_ACCOUNT_LIST);
        setSelectedAccountName(accountName);
    }

    /**
     * Notifies when the user clicked the "add account" button.
     */
    @Override
    public void addAccount() {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED);
        mAccountPickerDelegate.addAccount(accountName -> {
            mAddedAccountName = accountName;
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED);
            onAccountSelected(accountName, false);
        });
    }

    /**
     * Notifies when the user clicked the "Go Incognito mode" button.
     */
    @Override
    public void goIncognitoMode() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.INCOGNITO_INTERSTITIAL);
    }

    /**
     * Notifies when user clicks the back-press button.
     *
     * @return true if the listener handles the back press, false if not.
     */
    @Override
    public boolean onBackPressed() {
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.EXPANDED_ACCOUNT_LIST) {
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
            return true;
        } else if (viewState == ViewState.INCOGNITO_INTERSTITIAL) {
            mModel.set(
                    AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.EXPANDED_ACCOUNT_LIST);
            return true;
        } else {
            // The bottom sheet will be dismissed for all other view states
            return false;
        }
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        mAccountPickerDelegate.onDismiss();
        mProfileDataCache.removeObserver(mProfileDataSourceObserver);
        mAccountManagerFacade.removeObserver(mAccountsChangeObserver);
    }

    /**
     * Updates the collapsed account list when account list changes.
     *
     * Implements {@link AccountsChangeObserver}.
     */
    private void onAccountListUpdated() {
        List<Account> accounts = mAccountManagerFacade.tryGetGoogleAccounts();
        if (accounts.isEmpty()) {
            // If all accounts disappeared, no matter if the account list is collapsed or expanded,
            // we will go to the zero account screen.
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.NO_ACCOUNTS);
            mSelectedAccountName = null;
            mDefaultAccountName = null;
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA, null);
            return;
        }

        mDefaultAccountName = accounts.get(0).name;
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.NO_ACCOUNTS) {
            // When a non-empty account list appears while it is currently zero-account screen,
            // we should change the screen to collapsed account list and set the selected account
            // to the first account of the account list
            setSelectedAccountName(mDefaultAccountName);
            mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                    ViewState.COLLAPSED_ACCOUNT_LIST);
        } else if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST
                && AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
            // When it is already collapsed account list, we update the selected account only
            // when the current selected account name is no longer in the new account list
            setSelectedAccountName(mDefaultAccountName);
        }
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    /**
     * Implements {@link ProfileDataCache.Observer}.
     */
    private void updateSelectedAccountData(String accountName) {
        if (TextUtils.equals(mSelectedAccountName, accountName)) {
            mModel.set(AccountPickerBottomSheetProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountName));
        }
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        // Clicking on the selected account when the account list is collapsed will expand the
        // account list and make the account list visible
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.EXPANDED_ACCOUNT_LIST);
    }

    /**
     * Callback for the PropertyKey
     * {@link AccountPickerBottomSheetProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        @ViewState
        int viewState = mModel.get(AccountPickerBottomSheetProperties.VIEW_STATE);
        if (viewState == ViewState.COLLAPSED_ACCOUNT_LIST
                || viewState == ViewState.SIGNIN_GENERAL_ERROR) {
            signIn();
        } else if (viewState == ViewState.NO_ACCOUNTS) {
            addAccount();
        } else if (viewState == ViewState.SIGNIN_AUTH_ERROR) {
            updateCredentials();
        }
    }

    private void signIn() {
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, ViewState.SIGNIN_IN_PROGRESS);
        SigninMetricsUtils.logAccountConsistencyPromoShownCount(
                "Signin.AccountConsistencyPromoAction.SignedIn.Count");
        if (TextUtils.equals(mSelectedAccountName, mAddedAccountName)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT);
        } else if (TextUtils.equals(mSelectedAccountName, mDefaultAccountName)) {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT);
        } else {
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
        }
        SigninPreferencesManager.getInstance().clearAccountPickerBottomSheetActiveDismissalCount();
        new AsyncTask<String>() {
            @Override
            protected String doInBackground() {
                return mAccountManagerFacade.getAccountGaiaId(mSelectedAccountName);
            }

            @Override
            protected void onPostExecute(String accountGaiaId) {
                CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(
                        mSelectedAccountName, accountGaiaId);
                mAccountPickerDelegate.signIn(
                        coreAccountInfo, AccountPickerBottomSheetMediator.this::onSigninFailed);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void onSigninFailed(GoogleServiceAuthError error) {
        final @AccountConsistencyPromoAction int promoAction;
        final @ViewState int newViewState;
        if (error.getState() == State.INVALID_GAIA_CREDENTIALS) {
            promoAction = AccountConsistencyPromoAction.AUTH_ERROR_SHOWN;
            newViewState = ViewState.SIGNIN_AUTH_ERROR;
        } else {
            promoAction = AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN;
            newViewState = ViewState.SIGNIN_GENERAL_ERROR;
        }
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction);
        mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE, newViewState);
    }

    private void updateCredentials() {
        mAccountPickerDelegate.updateCredentials(mSelectedAccountName, (isSuccess) -> {
            if (isSuccess) {
                mModel.set(AccountPickerBottomSheetProperties.VIEW_STATE,
                        ViewState.COLLAPSED_ACCOUNT_LIST);
            }
        });
    }
}
