// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.autofill.autofill_ai.EntityInstance;

/** A delegate class to handle shared logic for Forms AI settings fragments. */
@NullMarked
public class FormsAiDelegate {
    private static final int DEFAULT_SNACKBAR_DURATION = 10000;

    private final ChromeBaseSettingsFragment mFragment;
    private final EntityEditorCoordinator.Delegate mEntityEditorDelegate =
            new EntityEditorCoordinator.Delegate() {
                @Override
                public void onDelete(EntityInstance entityInstance) {
                    EntityDataManager entityDataManager =
                            EntityDataManagerFactory.getForProfile(mFragment.getProfile());
                    if (entityDataManager == null) {
                        return;
                    }
                    entityDataManager.removeEntityInstance(entityInstance.getGUID());
                }

                @Override
                public void onDone(
                        EntityInstance entityInstance,
                        int descriptionStringId,
                        int acceptButtonStringId) {
                    EntityDataManager entityDataManager =
                            EntityDataManagerFactory.getForProfile(mFragment.getProfile());
                    if (entityDataManager == null) {
                        return;
                    }
                    entityDataManager.addOrUpdateEntityInstance(
                            entityInstance,
                            descriptionStringId,
                            acceptButtonStringId,
                            () -> onLocalSaveFallback());
                }

                @Override
                public void onOpenGoogleWallet(boolean isPrivateEntity) {
                    Context context = mFragment.getContext();
                    if (context == null) {
                        return;
                    }

                    if (isPrivateEntity) {
                        GoogleWalletLauncher.openGoogleWalletPrivatePassHelpCenterPage(context);
                    } else {
                        GoogleWalletLauncher.openGoogleWallet(context, context.getPackageManager());
                    }
                }
            };

    /**
     * @param fragment The fragment hosting the settings.
     */
    FormsAiDelegate(ChromeBaseSettingsFragment fragment) {
        mFragment = fragment;
    }

    EntityEditorCoordinator.Delegate getEntityEditorDelegate() {
        return mEntityEditorDelegate;
    }

    private void onLocalSaveFallback() {
        if (!(mFragment.getActivity() instanceof SnackbarManager.SnackbarManageable manageable)) {
            return;
        }

        @Nullable SnackbarManager snackbarManager = manageable.getSnackbarManager();
        if (snackbarManager == null) {
            return;
        }

        final String snackbarMessage =
                mFragment
                        .getActivity()
                        .getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_title);
        Snackbar snackBar =
                Snackbar.make(
                        snackbarMessage,
                        /* controller= */ null,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_AUTOFILL_AI_LOCAL_SAVE_FALLBACK);
        final String snackbarButton =
                mFragment
                        .getActivity()
                        .getString(
                                R.string
                                        .autofill_ai_save_or_update_entity_failed_wallet_save_dialog_confirmation_button_label);
        snackBar.setAction(snackbarButton, /* actionData= */ null);
        // Wrap the message text if it doesn't fit on a single line. The action text will not wrap
        // though.
        snackBar.setDefaultLines(false);
        snackBar.setDuration(DEFAULT_SNACKBAR_DURATION);
        snackbarManager.showSnackbar(snackBar);
    }
}
