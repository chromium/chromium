// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.frebottomgroup;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.signin.ui.frebottomgroup.FREBottomGroupCoordinator.Listener;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

class FREBottomGroupMediator implements AccountsChangeObserver, ProfileDataCache.Observer,
                                        AccountPickerCoordinator.Listener {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Listener mListener;
    private final PropertyModel mModel;
    // TODO(crbug/1227314): ProfileDataCache needs to be adjusted for supervised accounts users.
    private @NonNull ProfileDataCache mProfileDataCache;
    private AccountPickerDialogCoordinator mDialogCoordinator;
    private String mSelectedAccountName;

    FREBottomGroupMediator(
            Context context, ModalDialogManager modalDialogManager, Listener listener) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mListener = listener;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel = FREBottomGroupProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked, mListener::advanceToNextPage);

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

    /**
     * Implements {@link ProfileDataCache.Observer}.
     */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    @Override
    public void onAccountSelected(String accountName, boolean isDefaultAccount) {
        setSelectedAccountName(accountName);
        mDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        mListener.addAccount();
        mDialogCoordinator.dismissDialog();
    }

    /**
     * Callback for the PropertyKey
     * {@link FREBottomGroupProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
    }

    /**
     * Callback for the PropertyKey
     * {@link FREBottomGroupProperties#ON_CONTINUE_AS_CLICKED}.
     * TODO(crbug/1227313): Implement sign-in without sync.
     */
    private void onContinueAsClicked() {
        if (mSelectedAccountName == null) {
            mListener.addAccount();
        }
        mListener.advanceToNextPage();
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountName, accountEmail)) {
            mModel.set(FREBottomGroupProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    private void updateAccounts(List<Account> accounts) {
        if (accounts.isEmpty()) {
            mSelectedAccountName = null;
            mModel.set(FREBottomGroupProperties.SELECTED_ACCOUNT_DATA, null);
        } else if (mSelectedAccountName == null
                || AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
            setSelectedAccountName(accounts.get(0).name);
        }
    }
}
