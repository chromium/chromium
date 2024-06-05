// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Manages the close all tabs modal dialog. */
public class CloseAllTabsDialog {
    private CloseAllTabsDialog() {}

    /**
     * Shows a modal dialog to confirm or cancel the close all tabs action.
     * @param modalDialogManagerSupplier Provides access to the modal dialog manager.
     * @param tabModelSelector {@link TabModelSelector} to get the Incognito tab count to
     *         show in the regular mode dialog.
     * @param onCloseAll Invoked on a positive button input.
     */
    public static void show(
            Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull TabModelSelector tabModelSelector,
            Runnable onCloseAll) {
        assert modalDialogManagerSupplier.hasValue();
        final ModalDialogManager manager = modalDialogManagerSupplier.get();
        final boolean isIncognito = tabModelSelector.getCurrentModel().isIncognito();

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(
                            PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            onCloseAll.run();

                            RecordUserAction.record("MobileCloseAllTabsDialog.ClosedAllTabs");
                            manager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                            RecordUserAction.record("MobileCloseAllTabsDialog.Cancelled");
                            manager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                            RecordUserAction.record(
                                    "MobileCloseAllTabsDialog.CancelledWithTouchOutside");
                        }

                        final boolean closedAll =
                                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                        RecordHistogram.recordBooleanHistogram(
                                isIncognito
                                        ? "Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito"
                                        : "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito",
                                closedAll);
                    }
                };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(
                                        isIncognito
                                                ? R.string.close_all_tabs_dialog_title_incognito
                                                : R.string.close_all_tabs_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getDialogDescriptionString(context, tabModelSelector))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.close_all_tabs_and_groups_action))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP, true);
    }

    @VisibleForTesting
    static String getDialogDescriptionString(
            @NonNull Context context, @NonNull TabModelSelector tabModelSelector) {
        boolean isIncognito = tabModelSelector.getCurrentModel().isIncognito();
        int incognitoCount = tabModelSelector.getModel(/* incognito= */ true).getCount();

        if (isIncognito) {
            return context.getString(R.string.close_all_tabs_dialog_message_incognito);
        } else {
            return (incognitoCount > 0)
                    ? context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .close_all_tabs_and_groups_dialog_message_with_incognito_tabs,
                                    incognitoCount,
                                    incognitoCount)
                    : context.getString(R.string.close_all_tabs_and_groups_dialog_message);
        }
    }
}
