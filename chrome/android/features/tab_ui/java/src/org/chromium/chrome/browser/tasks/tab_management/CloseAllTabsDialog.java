// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Manages the alert dialog to confirm or cancel the "close all tabs" action. */
@NullMarked
public class CloseAllTabsDialog {
    private CloseAllTabsDialog() {}

    /**
     * Shows a modal dialog to confirm or cancel the close all tabs action, passing whether pinned
     * tabs should also be closed to the callback.
     *
     * @param modalDialogManagerSupplier Provides access to the modal dialog manager.
     * @param tabModelSelector {@link TabModelSelector} to get the Incognito and pinned tab counts.
     * @param onCloseAll Invoked on a positive button input with whether pinned tabs should also be
     *     closed.
     * @param isIncognitoOnly Whether to only close incognito tabs.
     */
    public static void show(
            Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            TabModelSelector tabModelSelector,
            Callback<Boolean> onCloseAll,
            boolean isIncognitoOnly) {
        assert modalDialogManagerSupplier.get() != null;
        final ModalDialogManager manager = modalDialogManagerSupplier.get();
        final TabModel regularModel = tabModelSelector.getModel(/* incognito= */ false);
        final boolean showCheckbox =
                !isIncognitoOnly
                        && ChromeFeatureList.sAndroidCloseAllTabsRetainsPinnedTabs.isEnabled()
                        && regularModel.getPinnedTabsCount() > 0;
        final boolean hasUnpinnedTabsToClose =
                TabModelUtils.hasUnpinnedTabs(regularModel)
                        || tabModelSelector.getModel(/* incognito= */ true).getCount() > 0;

        ModalDialogProperties.Controller controller =
                createController(manager, isIncognitoOnly, showCheckbox, onCloseAll);
        PropertyModel model =
                buildDialogModel(
                        context,
                        tabModelSelector,
                        controller,
                        isIncognitoOnly,
                        showCheckbox,
                        hasUnpinnedTabsToClose);
        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP, /* showAsNext= */ true);
    }

    private static ModalDialogProperties.Controller createController(
            ModalDialogManager manager,
            boolean isIncognitoOnly,
            boolean showCheckbox,
            Callback<Boolean> onCloseAll) {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, @ButtonType int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    boolean closePinnedTabs =
                            showCheckbox && model.get(ModalDialogProperties.CHECKBOX_CHECKED);
                    if (showCheckbox) {
                        RecordHistogram.recordBooleanHistogram(
                                "Tab.CloseAllTabsDialog.AlsoClosePinnedTabs", closePinnedTabs);
                    }
                    onCloseAll.onResult(closePinnedTabs);

                    RecordUserAction.record("MobileCloseAllTabsDialog.ClosedAllTabs");
                    manager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ButtonType.NEGATIVE) {
                    RecordUserAction.record("MobileCloseAllTabsDialog.Cancelled");
                    manager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                    RecordUserAction.record("MobileCloseAllTabsDialog.CancelledWithTouchOutside");
                }

                final boolean closedAll =
                        dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                RecordHistogram.recordBooleanHistogram(
                        isIncognitoOnly
                                ? "Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito"
                                : "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito",
                        closedAll);
            }
        };
    }

    private static PropertyModel buildDialogModel(
            Context context,
            TabModelSelector tabModelSelector,
            ModalDialogProperties.Controller controller,
            boolean isIncognitoOnly,
            boolean showCheckbox,
            boolean hasUnpinnedTabsToClose) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(
                                        isIncognitoOnly
                                                ? R.string.close_all_tabs_dialog_title_incognito
                                                : R.string.close_all_tabs_dialog_title))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getDialogDescriptionString(
                                        context, tabModelSelector, isIncognitoOnly))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.close_all_tabs_and_groups_action))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
        if (showCheckbox) {
            builder.with(
                            ModalDialogProperties.CHECKBOX_TEXT,
                            context.getString(
                                    R.string.close_all_tabs_dialog_also_close_pinned_tabs))
                    .with(ModalDialogProperties.CHECKBOX_CHECKED, false)
                    .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !hasUnpinnedTabsToClose);
        }

        PropertyModel model = builder.build();
        if (showCheckbox && !hasUnpinnedTabsToClose) {
            model.addObserver(
                    (source, propertyKey) -> {
                        if (propertyKey == ModalDialogProperties.CHECKBOX_CHECKED) {
                            model.set(
                                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                                    !model.get(ModalDialogProperties.CHECKBOX_CHECKED));
                        }
                    });
        }
        return model;
    }

    @VisibleForTesting
    static String getDialogDescriptionString(
            Context context, TabModelSelector tabModelSelector, boolean isIncognitoOnly) {
        int incognitoCount = tabModelSelector.getModel(/* incognito= */ true).getCount();

        if (isIncognitoOnly) {
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
