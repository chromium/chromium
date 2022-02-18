// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Manages the close all tabs modal dialog.
 */
public class CloseAllTabsDialog {
    @VisibleForTesting
    static final String SHOW_CANNOT_UNDO_WARNING = "show_cannot_undo_warning";

    private CloseAllTabsDialog() {}

    /**
     * Shows a modal dialog to confirm or cancel the close all tabs action.
     */
    public static void show(Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Runnable onCloseAll,
            boolean willExitOnCloseAll) {
        assert modalDialogManagerSupplier.hasValue();
        final ModalDialogManager manager = modalDialogManagerSupplier.get();

        // Show the cannot undo warning if the app will exit on close all and the param is enabled.
        final boolean showCannotUndoWarning = willExitOnCloseAll
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CLOSE_ALL_TABS_MODAL_DIALOG, SHOW_CANNOT_UNDO_WARNING,
                        true);

        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(
                    PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    onCloseAll.run();

                    RecordUserAction.record("MobileCloseAllTabsDialog.ClosedAllTabs");
                    manager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    RecordUserAction.record("MobileCloseAllTabsDialog.Cancelled");
                    manager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                    RecordUserAction.record("MobileCloseAllTabsDialog.CancelledWithTouchOutside");
                }

                // Assess whether a stricter warning has any impact on close all tabs behavior.
                RecordHistogram.recordBooleanHistogram("Tab.CloseAllTabsDialog.ClosedAllTabs."
                                + (showCannotUndoWarning
                                                ? "CannotUndoWarning"
                                                : (willExitOnCloseAll ? "NoWarningImmediateExit"
                                                                      : "Default")),
                        dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
        };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE,
                                context.getString(R.string.close_all_tabs_dialog_title))
                        .with(ModalDialogProperties.MESSAGE,
                                context.getString(showCannotUndoWarning
                                                ? R.string.close_all_tabs_dialog_warning_message
                                                : R.string.close_all_tabs_dialog_message))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.menu_close_all_tabs))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .build();

        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP, true);
    }
}
