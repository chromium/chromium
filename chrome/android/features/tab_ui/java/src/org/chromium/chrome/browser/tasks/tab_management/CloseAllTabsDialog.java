// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

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
    private CloseAllTabsDialog() {}

    /**
     * Shows a modal dialog to confirm or cancel the close all tabs action.
     * @param modalDialogManagerSupplier Provides access to the modal dialog manager.
     * @param onCloseAll Invoked on a positive button input.
     * @param isIncognito Whether to show incognito strings.
     */
    public static void show(Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier, Runnable onCloseAll,
            boolean isIncognito) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CLOSE_ALL_TABS_MODAL_DIALOG)) {
            onCloseAll.run();
            return;
        }

        assert modalDialogManagerSupplier.hasValue();
        final ModalDialogManager manager = modalDialogManagerSupplier.get();

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
                RecordHistogram.recordBooleanHistogram("Tab.CloseAllTabsDialog.ClosedAllTabs",
                        dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
        };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE,
                                context.getString(isIncognito
                                                ? R.string.close_all_tabs_dialog_title_incognito
                                                : R.string.close_all_tabs_dialog_title))
                        .with(ModalDialogProperties.MESSAGE,
                                context.getString(isIncognito
                                                ? R.string.close_all_tabs_dialog_message_incognito
                                                : R.string.close_all_tabs_dialog_message))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.menu_close_all_tabs))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP, true);
    }
}
