// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.generic_ui;

import static org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue.isDateSingleton;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.prefeditor.EditorTextField;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantDateTime;
import org.chromium.content.browser.input.PopupItemType;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupItem;
import org.chromium.content.browser.picker.InputDialogContainer;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** JNI bridge between {@code generic_ui_interactions_android} and Java. */
@JNINamespace("autofill_assistant")
public class AssistantViewInteractions {
    @CalledByNative
    private static void showListPopup(Context context, String[] itemNames,
            @PopupItemType int[] itemTypes, int[] selectedItems, boolean multiple,
            String selectedIndicesIdentifier, @Nullable String selectedNamesIdentifier,
            AssistantGenericUiDelegate delegate) {
        assert (itemNames.length == itemTypes.length);
        List<SelectPopupItem> popupItems = new ArrayList<>();
        for (int i = 0; i < itemNames.length; i++) {
            popupItems.add(new SelectPopupItem(itemNames[i], itemTypes[i]));
        }

        SelectPopupDialog dialog = new SelectPopupDialog(context, (indices) -> {
            AssistantValue selectedNamesValue = null;
            if (selectedNamesIdentifier != null) {
                String[] selectedNames = new String[indices != null ? indices.length : 0];
                for (int i = 0; i < selectedNames.length; ++i) {
                    selectedNames[i] = itemNames[indices[i]];
                }
                selectedNamesValue = new AssistantValue(selectedNames);
            }
            delegate.onValueChanged(selectedIndicesIdentifier, new AssistantValue(indices));
            if (!TextUtils.isEmpty(selectedNamesIdentifier)) {
                delegate.onValueChanged(selectedNamesIdentifier, selectedNamesValue);
            }
        }, popupItems, multiple, selectedItems);
        dialog.show();
    }

    @CalledByNative
    private static boolean showCalendarPopup(Context context, @Nullable AssistantValue initialDate,
            AssistantValue minDate, AssistantValue maxDate, String outputIdentifier,
            AssistantGenericUiDelegate delegate) {
        if ((initialDate != null && !isDateSingleton(initialDate)) || !isDateSingleton(minDate)
                || !isDateSingleton(maxDate)) {
            return false;
        }

        InputDialogContainer inputDialogContainer =
                new InputDialogContainer(context, new InputDialogContainer.InputActionDelegate() {
                    @Override
                    public void cancelDateTimeDialog() {
                        // Do nothing.
                    }

                    @Override
                    public void replaceDateTime(double value) {
                        // User tapped the 'clear' button.
                        if (Double.isNaN(value)) {
                            delegate.onValueChanged(outputIdentifier, null);
                        } else {
                            delegate.onValueChanged(outputIdentifier,
                                    AssistantValue.createForDateTimes(Collections.singletonList(
                                            new AssistantDateTime((long) value))));
                        }
                    }
                });

        inputDialogContainer.showDialog(org.chromium.ui.base.ime.TextInputType.DATE,
                initialDate != null ? initialDate.getDateTimes().get(0).getTimeInUtcMillis()
                                    : Double.NaN,
                minDate.getDateTimes().get(0).getTimeInUtcMillis(),
                maxDate.getDateTimes().get(0).getTimeInUtcMillis(), -1, null);
        return true;
    }

    @CalledByNative
    static boolean setViewText(View view, String text, AssistantGenericUiDelegate delegate) {
        if (view instanceof TextView) {
            AssistantTextUtils.applyVisualAppearanceTags(
                    (TextView) view, text, delegate::onTextLinkClicked);
            return true;
        } else if (view instanceof EditorTextField) {
            AssistantTextUtils.applyVisualAppearanceTags(
                    ((EditorTextField) view).getEditText(), text, delegate::onTextLinkClicked);
            return true;
        }
        return false;
    }

    @CalledByNative
    private static void setViewVisibility(View view, AssistantValue visible) {
        if (visible.getBooleans() != null && visible.getBooleans().length == 1) {
            view.setVisibility(visible.getBooleans()[0] ? View.VISIBLE : View.GONE);
        }
    }

    @CalledByNative
    private static void setViewEnabled(View view, AssistantValue enabled) {
        if (enabled.getBooleans() != null && enabled.getBooleans().length == 1) {
            view.setEnabled(enabled.getBooleans()[0]);
        }
    }

    @CalledByNative
    private static boolean setToggleButtonChecked(View view, AssistantValue checked) {
        if (!(view instanceof AssistantToggleButton)) {
            return false;
        }
        if (checked.getBooleans() == null || checked.getBooleans().length != 1) {
            return false;
        }
        ((AssistantToggleButton) view).setChecked(checked.getBooleans()[0]);
        return true;
    }

    @CalledByNative
    private static void showGenericPopup(View contentView, Context context,
            AssistantGenericUiDelegate delegate, String popupIdentifier) {
        new AlertDialog
                .Builder(context,
                        org.chromium.chrome.autofill_assistant.R.style.Theme_Chromium_AlertDialog)
                .setView(contentView)
                .setOnDismissListener(unused -> delegate.onGenericPopupDismissed(popupIdentifier))
                .show();
    }

    @CalledByNative
    private static boolean clearViewContainer(
            View container, String viewIdentifier, AssistantGenericUiDelegate delegate) {
        if (!(container instanceof ViewGroup)) {
            return false;
        }
        ((ViewGroup) container).removeAllViews();
        delegate.onViewContainerCleared(viewIdentifier);
        return true;
    }

    @CalledByNative
    private static boolean attachViewToParent(View parent, View view) {
        if (view == null || !(parent instanceof ViewGroup)) {
            return false;
        }
        if (view.getParent() != null) {
            return false;
        }
        ((ViewGroup) parent).addView(view);
        return true;
    }
}
