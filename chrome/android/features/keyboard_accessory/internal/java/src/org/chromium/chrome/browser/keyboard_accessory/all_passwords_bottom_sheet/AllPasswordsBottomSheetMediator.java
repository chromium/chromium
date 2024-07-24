// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * Contains the logic for the AllPasswordsBottomSheet. It sets the state of the model and reacts to
 * events like clicks.
 */
class AllPasswordsBottomSheetMediator {
    private AllPasswordsBottomSheetCoordinator.Delegate mDelegate;
    private PropertyModel mModel;
    private ListModel<ListItem> mListModel;
    private List<Credential> mCredentials;
    private boolean mIsPasswordField;

    void initialize(
            AllPasswordsBottomSheetCoordinator.Delegate delegate,
            PropertyModel model,
            ListModel<ListItem> listModel) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mListModel = listModel;
    }

    void showCredentials(List<Credential> credentials, boolean isPasswordField) {
        assert credentials != null;
        Collections.sort(credentials, AllPasswordsBottomSheetMediator::compareCredentials);

        mCredentials = credentials;
        mIsPasswordField = isPasswordField;

        mListModel.clear();

        for (Credential credential : mCredentials) {
            if (credential.getPassword().isEmpty() && isPasswordField) continue;
            final PropertyModel model =
                    AllPasswordsBottomSheetProperties.CredentialProperties.createCredentialModel(
                            credential, this::onCredentialSelected, mIsPasswordField);
            mListModel.add(
                    new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL, model));
        }
        mModel.set(VISIBLE, true);
    }

    /**
     * Filters the credentials list based on the passed text and adds the resulting credentials to
     * the model.
     *
     * @param newText the text used to filter the credentials.
     */
    void onQueryTextChange(String newText) {
        mListModel.clear();

        for (Credential credential : mCredentials) {
            if ((credential.getPassword().isEmpty() && mIsPasswordField)
                    || shouldBeFiltered(newText, credential)) {
                continue;
            }
            final PropertyModel model =
                    AllPasswordsBottomSheetProperties.CredentialProperties.createCredentialModel(
                            credential, this::onCredentialSelected, mIsPasswordField);
            mListModel.add(
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
                && !credential
                        .getOriginUrl()
                        .toLowerCase(Locale.ENGLISH)
                        .contains(searchQuery.toLowerCase(Locale.ENGLISH))
                && !credential
                        .getUsername()
                        .toLowerCase(Locale.getDefault())
                        .contains(searchQuery.toLowerCase(Locale.getDefault()));
    }

    void onCredentialSelected(CredentialFillRequest credentialFillRequest) {
        mModel.set(VISIBLE, false);
        mDelegate.onCredentialSelected(credentialFillRequest);
    }

    void onDismissed(@StateChangeReason Integer reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }

    private static int compareCredentials(Credential credential1, Credential credential2) {
        String displayOrigin1 =
                credential1.isAndroidCredential()
                        ? credential1.getAppDisplayName().toLowerCase(Locale.ENGLISH)
                        : UrlUtilities.getDomainAndRegistry(credential1.getOriginUrl(), false);
        String displayOrigin2 =
                credential2.isAndroidCredential()
                        ? credential2.getAppDisplayName().toLowerCase(Locale.ENGLISH)
                        : UrlUtilities.getDomainAndRegistry(credential2.getOriginUrl(), false);
        return displayOrigin1.compareTo(displayOrigin2);
    }
}
