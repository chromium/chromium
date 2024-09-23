// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetViewBinder.UiConfiguration;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

import java.util.List;

/**
 * Creates the AllPasswordsBottomSheet. AllPasswordsBottomSheet uses a bottom sheet to let the user
 * select a credential and fills it into the focused form.
 */
class AllPasswordsBottomSheetCoordinator {
    private final AllPasswordsBottomSheetMediator mMediator = new AllPasswordsBottomSheetMediator();

    /**
     * This delegate is called when the AllPasswordsBottomSheet is interacted with (e.g. dismissed
     * or a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user selects one of the credentials shown in the AllPasswordsBottomSheet.
         */
        void onCredentialSelected(CredentialFillRequest credentialFillRequest);

        /**
         * Called when the user dismisses the AllPasswordsBottomSheet or if the bottom sheet content
         * failed to be shown.
         */
        void onDismissed();
    }

    /**
     * Initializes the component.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param profile The {@link Profile} associated with the passwords.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles select and dismiss events.
     * @param origin The origin for the current focused frame.
     */
    public void initialize(
            Context context,
            Profile profile,
            BottomSheetController sheetController,
            Delegate delegate,
            String origin) {
        PropertyModel model =
                AllPasswordsBottomSheetProperties.createDefaultModel(
                        origin, mMediator::onDismissed, mMediator::onQueryTextChange);
        ListModel<ListItem> listModel = new ListModel<>();
        mMediator.initialize(delegate, model, listModel);

        UiConfiguration uiConfiguration = new UiConfiguration();
        uiConfiguration.faviconHelper = FaviconHelper.create(context, profile);
        setUpView(
                model,
                listModel,
                new AllPasswordsBottomSheetView(context, sheetController),
                uiConfiguration);
    }

    /**
     * Displays the given credentials in a new bottom sheet.
     *
     * @param credentials An array of {@link Credential}s that will be displayed.
     * @param isPasswordField True if the currently focused field is a password field and false for
     *     any other field type (e.g username, ...).
     */
    public void showCredentials(List<Credential> credentials, boolean isPasswordField) {
        mMediator.showCredentials(credentials, isPasswordField);
    }

    @VisibleForTesting
    static void setUpView(
            PropertyModel model,
            ListModel<ListItem> listModel,
            AllPasswordsBottomSheetView view,
            UiConfiguration uiConfiguration) {
        view.setSheetItemListAdapter(
                new RecyclerViewAdapter<>(
                        new SimpleRecyclerViewMcp<>(
                                listModel,
                                AllPasswordsBottomSheetProperties::getItemType,
                                AllPasswordsBottomSheetViewBinder::connectPropertyModel),
                        (parent, itemType) ->
                                AllPasswordsBottomSheetViewBinder.createViewHolder(
                                        parent, itemType, uiConfiguration)));
        PropertyModelChangeProcessor.create(
                model, view, AllPasswordsBottomSheetViewBinder::bindAllPasswordsBottomSheet);
    }
}
