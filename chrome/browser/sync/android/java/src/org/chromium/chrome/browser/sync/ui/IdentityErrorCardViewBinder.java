// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.content.Context;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.sync.R;
import org.chromium.chrome.browser.sync.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.SyncSettingsUtils.ErrorUiAction;
import org.chromium.components.sync.UserActionableError;

/** View binder for Identity Error Card. */
@NullMarked
public class IdentityErrorCardViewBinder {
    public static void bind(
            Context context,
            View errorCardView,
            @UserActionableError int identityError,
            Runnable buttonAction) {
        ImageView image = errorCardView.findViewById(R.id.signin_settings_card_icon);
        image.setContentDescription(
                context.getString(R.string.accessibility_account_management_row_account_error));
        image.setImageDrawable(AppCompatResources.getDrawable(context, R.drawable.ic_error));

        TextView description = errorCardView.findViewById(R.id.signin_settings_card_description);
        Button button = errorCardView.findViewById(R.id.signin_settings_card_button);

        updateErrorCardDetails(context, description, button, identityError);

        button.setOnClickListener(
                v -> {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Sync.IdentityErrorCard"
                                    + SyncSettingsUtils.getHistogramSuffixForError(identityError),
                            ErrorUiAction.BUTTON_CLICKED,
                            ErrorUiAction.NUM_ENTRIES);
                    buttonAction.run();
                });
    }

    private static void updateErrorCardDetails(
            Context context,
            TextView description,
            Button button,
            @UserActionableError int identityError) {
        @StringRes int message;
        @StringRes int buttonLabel;
        switch (identityError) {
            case UserActionableError.NEEDS_PASSPHRASE:
                message = R.string.identity_error_card_passphrase_required;
                buttonLabel = R.string.identity_error_card_button_passphrase_required;
                break;
            case UserActionableError.NEEDS_CLIENT_UPGRADE:
                message = R.string.identity_error_card_client_out_of_date;
                buttonLabel = R.string.identity_error_card_button_client_out_of_date;
                break;
            case UserActionableError.SIGN_IN_NEEDS_UPDATE:
                message = R.string.identity_error_card_auth_error;
                buttonLabel = R.string.identity_error_card_button_verify;
                break;
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                message = R.string.identity_error_card_sync_retrieve_keys_for_everything;
                buttonLabel = R.string.identity_error_card_button_verify;
                break;
            case UserActionableError.NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
                message = R.string.identity_error_card_sync_retrieve_keys_for_passwords;
                buttonLabel = R.string.identity_error_card_button_verify;
                break;
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING:
                message = R.string.identity_error_card_sync_recoverability_degraded_for_everything;
                buttonLabel = R.string.identity_error_card_button_verify;
                break;
            case UserActionableError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS:
                message = R.string.identity_error_card_sync_recoverability_degraded_for_passwords;
                buttonLabel = R.string.identity_error_card_button_verify;
                break;
            case UserActionableError.NEEDS_UPM_BACKEND_UPGRADE:
                message = R.string.sync_error_card_outdated_gms;
                buttonLabel = R.string.password_manager_outdated_gms_positive_button;
                break;
            case UserActionableError.BOOKMARKS_LIMIT_EXCEEDED:
                message = R.string.bookmark_sync_limit_error_description;
                buttonLabel = R.string.learn_more;
                break;
            case UserActionableError.NONE:
            default:
                assert false; // NOTREACHED()
                return;
        }
        description.setText(context.getString(message));
        button.setText(context.getString(buttonLabel));
    }
}
