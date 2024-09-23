// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebauthnCredential;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;

import java.util.List;

/**
 * This component allows to fill credentials into a form. It suppresses the keyboard until dismissed
 * and acts as a safe surface to fill credentials from.
 */
public interface TouchToFillComponent {
    /**
     * This delegate is called when the TouchToFill component is interacted with (e.g. dismissed or
     * a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user select one of the credentials shown in the TouchToFillComponent.
         *
         * @param credential The selected {@link Credential}.
         */
        void onCredentialSelected(Credential credential);

        /**
         * Called when the user select one of the Web Authentication credentials shown in the
         * TouchToFillComponent.
         *
         * @param credential The selected {@link WebauthnCredential}.
         */
        void onWebAuthnCredentialSelected(WebauthnCredential credential);

        /**
         * Called when the user dismisses the TouchToFillComponent. Not called if a suggestion was
         * selected.
         */
        void onDismissed();

        /**
         * Called when the user selects the "Manage Passwords" option.
         *
         * @param passkeysShown True when the sheet contained passkey credentials.
         */
        void onManagePasswordsSelected(boolean passkeysShown);

        /** Called when the user selects 'Use a Passkey on a Different Device'. */
        void onHybridSignInSelected();

        /** Called when the users selects 'More passkeys'. */
        void onShowMorePasskeysSelected();
    }

    /**
     * Initializes the component.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles dismiss events.
     * @param bottomSheetFocusHelper A {@link BottomSheetFocusHelper} used to restore accessibility
     *     focus after the BottomSheet closes.
     */
    void initialize(
            Context context,
            Profile profile,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper);

    /**
     * Displays the given credentials in a new bottom sheet.
     *
     * @param url A {@link String} that contains the URL to display credentials for.
     * @param isOriginSecure A {@link boolean} that indicates whether the current origin is secure.
     * @param webauthnCredentials A list of {@link WebauthnCredential}s that will be displayed.
     * @param credentials A list of {@link Credential}s that will be displayed.
     * @param triggerSubmission A {@link boolean} that indicates whether a form should be submitted
     *     after filling.
     * @param managePasskeysHidesPasswords A {@link boolean} that indicates whether managing
     *     passkeys will show a screen that does not provide password management.
     * @param showHybridPasskeyOption A {@link boolean} that indicates whether the footer should
     *     display an option to initiate hybrid sign-in.
     * @param showCredManEntry A {@link boolean} that indicates whether the list should have an item
     *     to open Android Credential Manager UI.
     */
    void showCredentials(
            GURL url,
            boolean isOriginSecure,
            List<WebauthnCredential> webauthnCredentials,
            List<Credential> credentials,
            boolean triggerSubmission,
            boolean managePasskeysHidesPasswords,
            boolean showHybridPasskeyOption,
            boolean showCredManEntry);
}
