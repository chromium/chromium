// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import android.accounts.Account;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.IncognitoAccountRowProperties;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerProperties.ItemType;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The mediator of account picker handles all the signals from the outside world.
 *
 * It defines the business logic when the user selects or adds an account and updates the model.
 * This class has no visibility of the account picker view.
 */
class AccountPickerMediator {
    private final MVCListAdapter.ModelList mListModel;
    private final AccountPickerCoordinator.Listener mAccountPickerListener;
    private final ProfileDataCache mProfileDataCache;
    private final boolean mShowIncognitoRow;
    private @Nullable String mSelectedAccountName;

    private final AccountManagerFacade mAccountManagerFacade;
    private final AccountsChangeObserver mAccountsChangeObserver = this::updateAccounts;
    private final ProfileDataCache.Observer mProfileDataObserver = this::updateProfileData;

    @MainThread
    AccountPickerMediator(Context context, MVCListAdapter.ModelList listModel,
            AccountPickerCoordinator.Listener listener, @Nullable String selectedAccountName,
            boolean showIncognitoRow) {
        mListModel = listModel;
        mAccountPickerListener = listener;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mShowIncognitoRow = showIncognitoRow;
        mSelectedAccountName = selectedAccountName;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        mAccountManagerFacade.addObserver(mAccountsChangeObserver);
        mProfileDataCache.addObserver(mProfileDataObserver);
        updateAccounts();
    }

    /**
     * Unregisters the observers used by the mediator.
     */
    @MainThread
    void destroy() {
        mProfileDataCache.removeObserver(mProfileDataObserver);
        mAccountManagerFacade.removeObserver(mAccountsChangeObserver);
    }

    void setSelectedAccountName(String selectedAccountName) {
        if (!TextUtils.equals(mSelectedAccountName, selectedAccountName)) {
            mSelectedAccountName = selectedAccountName;
            updateSelectedAccount();
        }
    }

    /**
     * Implements {@link AccountsChangeObserver}.
     */
    private void updateAccounts() {
        List<Account> accounts = mAccountManagerFacade.tryGetGoogleAccounts();
        mListModel.clear();

        // Add an "existing account" row for each account
        if (accounts.size() > 0) {
            DisplayableProfileData profileData =
                    mProfileDataCache.getProfileDataOrDefault(accounts.get(0).name);
            mListModel.add(createExistingAccountRowItem(profileData, true));
            for (int i = 1; i < accounts.size(); ++i) {
                profileData = mProfileDataCache.getProfileDataOrDefault(accounts.get(i).name);
                mListModel.add(createExistingAccountRowItem(profileData, false));
            }
        }

        // Add an "add account" row
        PropertyModel model =
                AddAccountRowProperties.createModel(mAccountPickerListener::addAccount);
        mListModel.add(new MVCListAdapter.ListItem(ItemType.ADD_ACCOUNT_ROW, model));

        // Add a "Go incognito mode" row
        if (mShowIncognitoRow) {
            PropertyModel incognitoModel = IncognitoAccountRowProperties.createModel(
                    mAccountPickerListener::goIncognitoMode);
            mListModel.add(
                    new MVCListAdapter.ListItem(ItemType.INCOGNITO_ACCOUNT_ROW, incognitoModel));
        }
    }

    private MVCListAdapter.ListItem createExistingAccountRowItem(
            DisplayableProfileData profileData, boolean isDefaultAccount) {
        Callback<DisplayableProfileData> profileDataCallback = profileData1
                -> mAccountPickerListener.onAccountSelected(
                        profileData1.getAccountEmail(), isDefaultAccount);
        PropertyModel model = ExistingAccountRowProperties.createModel(profileData,
                profileDataCallback, profileData.getAccountEmail().equals(mSelectedAccountName));
        return new MVCListAdapter.ListItem(ItemType.EXISTING_ACCOUNT_ROW, model);
    }

    private void updateSelectedAccount() {
        for (MVCListAdapter.ListItem item : mListModel) {
            if (item.type == AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW) {
                PropertyModel model = item.model;
                boolean isSelectedAccount = TextUtils.equals(mSelectedAccountName,
                        model.get(ExistingAccountRowProperties.PROFILE_DATA).getAccountEmail());
                model.set(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT, isSelectedAccount);
            }
        }
    }

    /**
     * Implements {@link ProfileDataCache.Observer}
     */
    private void updateProfileData(String accountName) {
        for (MVCListAdapter.ListItem item : mListModel) {
            if (item.type == AccountPickerProperties.ItemType.EXISTING_ACCOUNT_ROW) {
                PropertyModel model = item.model;
                boolean isProfileDataUpdated = TextUtils.equals(accountName,
                        model.get(ExistingAccountRowProperties.PROFILE_DATA).getAccountEmail());
                if (isProfileDataUpdated) {
                    model.set(ExistingAccountRowProperties.PROFILE_DATA,
                            mProfileDataCache.getProfileDataOrDefault(accountName));
                    break;
                }
            }
        }
    }
}
