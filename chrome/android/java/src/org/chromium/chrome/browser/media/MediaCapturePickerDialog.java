// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog for selecting a media source for media capture. */
public class MediaCapturePickerDialog {
    private final ModalDialogManager mModalDialogManager;
    private Callback<WebContents> mCallback;

    /**
     * Shows the media capture picker dialog.
     *
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param callback Invoked with a WebContents if a tab is selected, or {@code null} if the
     *     dialog is dismissed.
     */
    public static void showDialog(
            ModalDialogManager modalDialogManager, Callback<WebContents> callback) {
        new MediaCapturePickerDialog(modalDialogManager, callback).show();
    }

    private MediaCapturePickerDialog(
            ModalDialogManager modalDialogManager, Callback<WebContents> callback) {
        mModalDialogManager = modalDialogManager;
        mCallback = callback;
    }

    private void show() {
        var allTabObserver =
                new AllTabObserver(
                        new AllTabObserver.Observer() {
                            @Override
                            public void onTabAdded(Tab tab) {
                                // TODO(crbug.com/352186941): Plumb this to the dialog.
                            }

                            @Override
                            public void onTabRemoved(Tab tab) {
                                // TODO(crbug.com/352186941): Plumb this to the dialog.
                            }
                        });

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        boolean picked = buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        // TODO(crbug.com/352186941): Return a `WebContents`.
                        mCallback.onResult(null);
                        mCallback = null;
                        mModalDialogManager.dismissDialog(
                                model,
                                picked
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (mCallback != null) {
                            mCallback.onResult(null);
                            mCallback = null;
                        }
                        allTabObserver.destroy();
                    }
                };

        var propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, "Share tab")
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "Share")
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, "Cancel")
                        .build();

        mModalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
    }
}
