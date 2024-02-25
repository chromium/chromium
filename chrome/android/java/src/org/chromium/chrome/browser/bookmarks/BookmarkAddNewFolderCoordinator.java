// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Class for showing a dialog to add a new folder. */
public class BookmarkAddNewFolderCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final BookmarkModel mBookmarkModel;

    private PropertyModel mModel;

    /**
     * @param context The android context from which this is being called.
     * @param modalDialogManager The manager for modal dialogs.
     * @param bookmarkModel The underlying bookmark model.
     */
    public BookmarkAddNewFolderCoordinator(
            Context context, ModalDialogManager modalDialogManager, BookmarkModel bookmarkModel) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mBookmarkModel = bookmarkModel;
    }

    /**
     * Show the dialog for the given parent. If the parent is the root, then the folder will be
     * created under "other bookmarks".
     */
    public void show(BookmarkId parent) {
        View customView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.bookmark_add_new_folder_input_layout, null);
        BookmarkTextInputLayout folderTitle = customView.findViewById(R.id.folder_title);

        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE
                                && !folderTitle.validate()) {
                            folderTitle.requestFocus();
                            return;
                        }

                        final @DialogDismissalCause int cause;
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            addFolder(parent, folderTitle.getTrimmedText());
                            cause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                        } else {
                            cause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
                        }

                        mModalDialogManager.dismissDialog(mModel, cause);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };

        Resources res = mContext.getResources();
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                res.getString(R.string.app_banner_add))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                res.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .build();
        mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    private void addFolder(BookmarkId parent, String title) {
        mBookmarkModel.addFolder(
                parent.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getOtherFolderId()
                        : parent,
                0,
                title);
    }
}
