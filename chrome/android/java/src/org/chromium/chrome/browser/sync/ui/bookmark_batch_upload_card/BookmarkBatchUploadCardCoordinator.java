// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.bookmark_batch_upload_card;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.lifecycle.LifecycleOwner;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class BookmarkBatchUploadCardCoordinator {
    private final BookmarkBatchUploadCardMediator mMediator;
    private final PropertyModel mModel;

    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public BookmarkBatchUploadCardCoordinator(
            Context context,
            Profile profile,
            SnackbarManager snackbarManager,
            Runnable batchUploadCardChangeAction) {

        mModel =
                new PropertyModel.Builder(BookmarkBatchUploadCardProperties.ALL_KEYS)
                        .with(
                                BookmarkBatchUploadCardProperties.BUTTON_TEXT,
                                R.string.bookmarks_left_behind_bookmarks_button)
                        .with(
                                BookmarkBatchUploadCardProperties.ICON,
                                UiUtils.getTintedDrawable(
                                        context,
                                        R.drawable.ic_cloud_upload_24dp,
                                        R.color.default_icon_color_accent1_tint_list))
                        .build();

        mMediator =
                new BookmarkBatchUploadCardMediator(
                        (Activity) context,
                        (LifecycleOwner) context,
                        (ModalDialogManagerHolder) context,
                        profile,
                        mModel,
                        snackbarManager,
                        batchUploadCardChangeAction);
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
                PropertyModelChangeProcessor.create(
                        mModel, view, BookmarkBatchUploadCardBinder::bind);
    }

    public void hideBatchUploadCardAndUpdate() {
        mMediator.hideBatchUploadCardAndUpdate();
    }

    public boolean shouldShowBatchUploadCard() {
        return mMediator.shouldBeVisible();
    }
}
