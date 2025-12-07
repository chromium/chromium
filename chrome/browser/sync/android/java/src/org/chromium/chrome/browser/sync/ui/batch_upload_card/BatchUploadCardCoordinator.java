// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.batch_upload_card;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.R;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class BatchUploadCardCoordinator {
    private final BatchUploadCardMediator mMediator;
    private final PropertyModel mModel;

    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @IntDef({EntryPoint.SETTINGS, EntryPoint.BOOKMARK_MANAGER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPoint {
        int SETTINGS = 0;
        int BOOKMARK_MANAGER = 1;
    }

    public BatchUploadCardCoordinator(
            Activity activity,
            LifecycleOwner lifecycleOwner,
            ModalDialogManager modalDialogManager,
            Profile profile,
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier,
            Runnable batchUploadCardChangeAction,
            @EntryPoint int entryPoint) {

        mModel =
                new PropertyModel.Builder(BatchUploadCardProperties.ALL_KEYS)
                        .with(
                                BatchUploadCardProperties.BUTTON_TEXT,
                                R.string.batch_upload_card_save_button)
                        .with(
                                BatchUploadCardProperties.ICON,
                                UiUtils.getTintedDrawable(
                                        activity,
                                        R.drawable.ic_cloud_upload_24dp,
                                        R.color.default_icon_color_accent1_tint_list))
                        .build();

        mMediator =
                new BatchUploadCardMediator(
                        activity,
                        lifecycleOwner,
                        modalDialogManager,
                        profile,
                        mModel,
                        snackbarManagerSupplier,
                        batchUploadCardChangeAction,
                        entryPoint);
    }

    public void destroy() {
        mMediator.destroy();
    }

    public void setView(View view) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, view, BatchUploadCardBinder::bind);
    }

    public void immediatelyHideBatchUploadCardAndUpdateItsVisibility() {
        mMediator.immediatelyHideBatchUploadCardAndUpdateItsVisibility();
    }

    public boolean shouldShowBatchUploadCard() {
        return mMediator.shouldBeVisible();
    }
}
