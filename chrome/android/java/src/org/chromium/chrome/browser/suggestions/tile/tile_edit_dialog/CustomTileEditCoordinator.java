// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditDelegates.MediatorToBrowser;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.url.GURL;

/** The Coordinator of the Custom Tile Edit Dialog. */
@NullMarked
public class CustomTileEditCoordinator implements MediatorToBrowser {
    /** Interface for handling tile values changes. */
    public interface TileValueChangeHandler {
        /**
         * Callback to receive updated tile values on user submit.
         *
         * @param name The new title of the tile.
         * @param url The new URL of the tile.
         * @return Whether the values are acceptable.
         */
        boolean onTileValueChange(String name, GURL url);
    }

    /** Interface for handling tile values changes. */
    public interface CustomLinksDuplicateChecker {
        /**
         * Checks whether the a URL already exists in a custom tile.
         *
         * @param url The URL to check.
         * @return True if the URL is a duplicate, false otherwise.
         */
        boolean isUrlDuplicate(GURL url);
    }

    private final ModalDialogManager mModalDialogManager;

    private final CustomTileEditView mView;
    private final CustomTileEditMediator mMediator;

    private TileValueChangeHandler mTileValueChangeHandler;
    private CustomLinksDuplicateChecker mCustomLinksDuplicateChecker;

    /**
     * Instantiates the class for production.
     *
     * @param modalDialogManager The modal dialog manager.
     * @param context The application context.
     * @param originalTile The tile being edited, or null if adding a new tile.
     */
    public static CustomTileEditCoordinator make(
            ModalDialogManager modalDialogManager, Context context, @Nullable Tile originalTile) {
        CustomTileEditView view =
                (CustomTileEditView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.custom_tile_edit_layout, null);
        CustomTileEditMediator mediator = new CustomTileEditMediator(originalTile);
        return new CustomTileEditCoordinator(modalDialogManager, view, mediator);
    }

    /**
     * @param modalDialogManager The modal dialog manager.
     * @param view The View of Custom Tile Edit Dialog.
     * @param mediator The Mediator of Custom Tile Edit Dialog.
     */
    public CustomTileEditCoordinator(
            ModalDialogManager modalDialogManager,
            CustomTileEditView view,
            CustomTileEditMediator mediator) {
        mModalDialogManager = modalDialogManager;
        mView = view;
        mMediator = mediator;
        mView.setMediatorDelegate(mMediator);
        mMediator.setDelegates(mView, this);
    }

    /**
     * Main entrance point to shows the dialog.
     *
     * @param tileValueChangeHandler Handler for value change submission.
     * @param customUrlDuplicateChecker Handler to determine whether input URL is a duplicate.
     */
    @Initializer
    public void show(
            TileValueChangeHandler tileValueChangeHandler,
            CustomLinksDuplicateChecker customUrlDuplicateChecker) {
        assert mTileValueChangeHandler == null;
        mTileValueChangeHandler = tileValueChangeHandler;
        assert mCustomLinksDuplicateChecker == null;
        mCustomLinksDuplicateChecker = customUrlDuplicateChecker;
        mMediator.show();
    }

    // CustomTileEditDelegates.MediatorToBrowser implementation.
    @Override
    public void showEditDialog() {
        mModalDialogManager.showDialog(mView.getDialogModel(), ModalDialogType.APP);
    }

    @Override
    public void closeEditDialog(boolean isSubmit) {
        final @DialogDismissalCause int cause =
                isSubmit
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        mModalDialogManager.dismissDialog(mView.getDialogModel(), cause);
    }

    @Override
    public boolean isUrlDuplicate(GURL url) {
        return mCustomLinksDuplicateChecker.isUrlDuplicate(url);
    }

    @Override
    public boolean submitChange(String title, GURL url) {
        return mTileValueChangeHandler.onTileValueChange(title, url);
    }
}
