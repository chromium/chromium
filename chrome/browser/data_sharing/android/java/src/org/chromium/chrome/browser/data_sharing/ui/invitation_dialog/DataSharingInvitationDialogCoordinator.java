// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.invitation_dialog;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;


/**
 * Coordinator class for displaying the data sharing invitation modal dialog. It proxies the
 * communication to the {@link DataSharingInvitationDialogMediator}.
 */
public class DataSharingInvitationDialogCoordinator {
    private final Context mContext;
    private final DataSharingInvitationDialogMediator mMediator;
    private final LinearLayout mButtonsView;

    /**
     * Constructor for {@link DataSharingInvitationDialogCoordinator}.
     *
     * @param context The context for accessing resources.
     * @param modalDialogMediator The DataSharingInvitationDialogMediator to control the dialog.
     */
    public DataSharingInvitationDialogCoordinator(
            Context context, @NonNull ModalDialogManager modalDialogManager) {
        mContext = context;
        mMediator = new DataSharingInvitationDialogMediator(modalDialogManager);
        mButtonsView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.data_sharing_invitation_dialog_footer, null);
    }

    /** Show the dialog */
    public void show() {
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, mMediator)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getResources(),
                                R.string.data_sharing_invitation_dialog_title)
                        .with(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mButtonsView)
                        .build();
        mMediator.show(dialogModel);
    }

    /** Dismisses the currently visible dialog. */
    public void dismiss() {
        mMediator.dismiss();
    }
}
