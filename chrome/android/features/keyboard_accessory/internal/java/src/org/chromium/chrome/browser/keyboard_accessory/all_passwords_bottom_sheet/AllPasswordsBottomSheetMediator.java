// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/**
 * Contains the logic for the AllPasswordsBottomSheet. It sets the state of the model and reacts to
 * events like clicks.
 */
class AllPasswordsBottomSheetMediator {
    private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;
    private PropertyModel mModel;
    private Credential[] mCredentials;
    private boolean mIsPasswordField;

    void initialize(AllPasswordsBottomSheetCoordinator.Delegate delegate, PropertyModel model) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
    }

    void showCredentials(Credential[] credentials, boolean isPasswordField) {
        assert credentials != null;
        mCredentials = credentials;
        mIsPasswordField = isPasswordField;

        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (Credential credential : mCredentials) {
            final PropertyModel model =
                    AllPasswordsBottomSheetProperties.CredentialProperties.createCredentialModel(
                            credential, this::onCredentialSelected, isPasswordField);
            sheetItems.add(
                    new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL, model));
        }

        mModel.set(VISIBLE, true);
    }

    /**
     * Filters the credentials list based on the passed text and adds the resulting credentials to
     * the model.
     * @param newText the text used to filter the credentials.
     */
    void onQueryTextChange(String newText) {
        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        for (Credential credential : mCredentials) {
            if (shouldBeFiltered(newText, credential)) continue;
            final PropertyModel model =
                    AllPasswordsBottomSheetProperties.CredentialProperties.createCredentialModel(
                            credential, this::onCredentialSelected, mIsPasswordField);
            sheetItems.add(
                    new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL, model));
        }
    }

    /**
     * Returns true if no substring in the passed credential matches the searchQuery ignoring the
     * characters case.
     * @param searchQuery the text to check if passed credential has it.
     * @param credential its username and origin will be checked for matching string.
     * @return Returns whether the entry with the passed credential should be filtered.
     */
    private boolean shouldBeFiltered(final String searchQuery, final Credential credential) {
        return searchQuery != null
                && !credential.getOriginUrl()
                            .toLowerCase(Locale.ENGLISH)
                            .contains(searchQuery.toLowerCase(Locale.ENGLISH))
                && !credential.getUsername()
                            .toLowerCase(Locale.getDefault())
                            .contains(searchQuery.toLowerCase(Locale.getDefault()));
    }

    void onCredentialSelected(Credential credential) {
        mModel.set(VISIBLE, false);
        mDelegate.onCredentialSelected(credential);
    }

    void onDismissed(Integer integer) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
