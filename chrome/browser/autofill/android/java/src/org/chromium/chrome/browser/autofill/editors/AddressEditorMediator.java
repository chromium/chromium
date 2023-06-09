// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AutofillProfileBridge;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.Source;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.List;

/**
 * Contains the logic for the AddressEditor component. It sets the state of the model and reacts to
 * events like address country selection.
 */
class AddressEditorMediator {
    private Context mContext;
    private Profile mProfile;
    private boolean mIsUpdate;
    private boolean mIsMigrationToAccount;
    private boolean mIsProfileNew;

    // TODO(crbug.com/1432505): remove temporary unsupported countries filtering.
    static List<DropdownKeyValue> getSupportedCountries(boolean filterOutUnsupportedCountries) {
        List<DropdownKeyValue> supportedCountries = AutofillProfileBridge.getSupportedCountries();
        if (filterOutUnsupportedCountries) {
            PersonalDataManager personalDataManager = PersonalDataManager.getInstance();
            supportedCountries.removeIf(entry
                    -> !personalDataManager.isCountryEligibleForAccountStorage(entry.getKey()));
        }

        return supportedCountries;
    }

    void initialize(Context context, Profile profile, boolean isUpdate,
            boolean isMigrationToAccount, boolean isProfileNew) {
        mContext = context;
        mProfile = profile;
        mIsUpdate = isUpdate;
        mIsMigrationToAccount = isMigrationToAccount;
        mIsProfileNew = isProfileNew;
    }

    boolean isProfileNew() {
        return mIsProfileNew;
    }

    boolean willBeSavedInAccount(@Source int source) {
        if (mIsMigrationToAccount) {
            return true;
        }

        if (source == Source.ACCOUNT && !mIsUpdate) {
            return true; // Only already saved address can be updated.
        }

        // User creates a new address profile, which is going to be stored in their Google account
        // according to the storage eligibility.
        return mIsProfileNew
                && PersonalDataManager.getInstance().isEligibleForAddressAccountStorage();
    }

    boolean isAccountAddressProfile(@Source int source) {
        return willBeSavedInAccount(source) || isAlreadySavedInAccount(source);
    }

    String getEditorTitle() {
        return mIsProfileNew ? mContext.getString(R.string.autofill_create_profile)
                             : mContext.getString(R.string.autofill_edit_address_dialog_title);
    }

    @Nullable
    String getUserEmail() {
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        CoreAccountInfo accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return CoreAccountInfo.getEmailFrom(accountInfo);
    }

    @Nullable
    String getDeleteConfirmationText(@Source int source) {
        if (isAccountAddressProfile(source)) {
            @Nullable
            String email = getUserEmail();
            if (email == null) return null;
            return mContext.getString(R.string.autofill_delete_account_address_source_notice)
                    .replace("$1", email);
        }
        if (isAddressSyncOn()) {
            return mContext.getString(R.string.autofill_delete_sync_address_source_notice);
        }
        return mContext.getString(R.string.autofill_delete_local_address_source_notice);
    }

    @Nullable
    String getSourceNoticeText(@Source int source) {
        if (!isAccountAddressProfile(source)) return null;
        @Nullable
        String email = getUserEmail();
        if (email == null) return null;

        if (isAlreadySavedInAccount(source)) {
            return mContext
                    .getString(R.string.autofill_address_already_saved_in_account_source_notice)
                    .replace("$1", email);
        }

        return mContext.getString(R.string.autofill_address_will_be_saved_in_account_source_notice)
                .replace("$1", email);
    }

    String getDeleteConfirmationTitle() {
        return mContext.getString(R.string.autofill_delete_address_confirmation_dialog_title);
    }

    private boolean isAlreadySavedInAccount(@Source int source) {
        return source == Source.ACCOUNT && mIsUpdate;
    }

    private boolean isAddressSyncOn() {
        SyncService service = SyncServiceFactory.get();
        if (service == null) return false;
        return service.isSyncFeatureEnabled()
                && service.getSelectedTypes().contains(UserSelectableType.AUTOFILL);
    }
}
