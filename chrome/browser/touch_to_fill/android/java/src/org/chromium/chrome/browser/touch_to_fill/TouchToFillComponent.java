// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * This component allows to fill credentials into a form. It suppresses the keyboard until dismissed
 * and acts as a safe surface to fill credentials from.
 */
public interface TouchToFillComponent {
    /**
     * The different reasons that the sheet's state can change.
     *
     * These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. Needs to stay in sync with TouchToFill.UserAction in enums.xml and
     * UserAction in touch_to_fill_controller.h.
     * TODO(crbug.com/1013134): Deduplicate the Java and C++ enum.
     */
    @IntDef({UserAction.SELECT_CREDENTIAL, UserAction.DISMISS, UserAction.SELECT_MANAGE_PASSWORDS,
            UserAction.SELECT_WEBAUTHN_CREDENTIAL, UserAction.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    @interface UserAction {
        int SELECT_CREDENTIAL = 0;
        int DISMISS = 1;
        int SELECT_MANAGE_PASSWORDS = 2;
        int SELECT_WEBAUTHN_CREDENTIAL = 3;
        int MAX_VALUE = SELECT_WEBAUTHN_CREDENTIAL;
    }

    /**
     * This delegate is called when the TouchToFill component is interacted with (e.g. dismissed or
     * a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user select one of the credentials shown in the TouchToFillComponent.
         * @param credential The selected {@link Credential}.
         */
        void onCredentialSelected(Credential credential);

        /**
         * Called when the user select one of the Web Authentication credentials shown in the
         * TouchToFillComponent.
         * @param credential The selected {@link WebAuthnCredential}.
         */
        void onWebAuthnCredentialSelected(WebAuthnCredential credential);

        /**
         * Called when the user dismisses the TouchToFillComponent. Not called if a suggestion was
         * selected.
         */
        void onDismissed();

        /**
         * Called when the user selects the "Manage Passwords" option.
         */
        void onManagePasswordsSelected();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles dismiss events.
     */
    void initialize(Context context, BottomSheetController sheetController, Delegate delegate);

    /**
     * Displays the given credentials in a new bottom sheet.
     * @param url A {@link String} that contains the URL to display credentials for.
     * @param isOriginSecure A {@link boolean} that indicates whether the current origin is secure.
     * @param webauthnCredentials A list of {@link WebAuthnCredential}s that will be displayed.
     * @param credentials A list of {@link Credential}s that will be displayed.
     * @param triggerSubmission A {@link boolean} that indicates whether a form should be submitted
     *         after filling.
     */
    void showCredentials(GURL url, boolean isOriginSecure,
            List<WebAuthnCredential> webauthnCredentials, List<Credential> credentials,
            boolean triggerSubmission);
}
