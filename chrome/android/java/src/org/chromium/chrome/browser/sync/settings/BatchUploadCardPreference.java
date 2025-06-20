// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.content.Context;
import android.util.AttributeSet;

import androidx.lifecycle.LifecycleOwner;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ui.batch_upload_card.BatchUploadCardCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

@NullMarked
public class BatchUploadCardPreference extends Preference {
    private BatchUploadCardCoordinator mBatchUploadCardCoordinator;

    public BatchUploadCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        setLayoutResource(R.layout.signin_settings_card_view);
    }

    /** Initialize the dependencies for the BatchUploadCardPreference and update the error card. */
    @Initializer
    public void initialize(
            Activity activity,
            Profile profile,
            ModalDialogManager dialogManager,
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mBatchUploadCardCoordinator =
                new BatchUploadCardCoordinator(
                        activity,
                        (LifecycleOwner) activity,
                        dialogManager,
                        profile,
                        snackbarManagerSupplier,
                        this::updateBatchUploadCard,
                        BatchUploadCardCoordinator.EntryPoint.SETTINGS);
    }

    public void hideBatchUploadCardAndUpdate() {
        // Temporarily hide, it will become visible again once getLocalDataDescriptions() completes,
        // which is triggered from update().
        mBatchUploadCardCoordinator.immediatelyHideBatchUploadCardAndUpdateItsVisibility();
    }

    @Override
    public void onDetached() {
        super.onDetached();
        mBatchUploadCardCoordinator.destroy();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        holder.setDividerAllowedAbove(false);
        mBatchUploadCardCoordinator.setView(holder.findViewById(R.id.signin_settings_card));
    }

    private void updateBatchUploadCard() {
        if (mBatchUploadCardCoordinator == null) {
            // Coordinator is not yet initialized. This can happen during the initial setup if the
            // callback is invoked before the mBatchUploadCardCoordinator field is assigned.
            return;
        }
        setVisible(mBatchUploadCardCoordinator.shouldShowBatchUploadCard());
        notifyChanged();
    }
}
