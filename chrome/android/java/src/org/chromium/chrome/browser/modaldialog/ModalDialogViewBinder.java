// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.text.TextUtils;

import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for binding view properties from {@link ModalDialogProperties} to a
 * {@link ModalDialogView}.
 */
public class ModalDialogViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<PropertyModel, ModalDialogView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, ModalDialogView view, PropertyKey propertyKey) {
        if (ModalDialogProperties.TITLE == propertyKey) {
            view.setTitle(model.get(ModalDialogProperties.TITLE));
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            view.setTitleIcon(model.get(ModalDialogProperties.TITLE_ICON));
        } else if (ModalDialogProperties.MESSAGE == propertyKey) {
            view.setMessage(model.get(ModalDialogProperties.MESSAGE));
        } else if (ModalDialogProperties.CUSTOM_VIEW == propertyKey) {
            view.setCustomView(model.get(ModalDialogProperties.CUSTOM_VIEW));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            view.setButtonText(ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(ModalDialogProperties.ButtonType.POSITIVE,
                    model.get(ModalDialogProperties.POSITIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.POSITIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogProperties.ButtonType.POSITIVE,
                    !model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_TEXT == propertyKey) {
            assert checkFilterTouchConsistency(model);
            assert checkFilledButtonConsistency(model);
            view.setButtonText(ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setButtonContentDescription(ModalDialogProperties.ButtonType.NEGATIVE,
                    model.get(ModalDialogProperties.NEGATIVE_BUTTON_CONTENT_DESCRIPTION));
        } else if (ModalDialogProperties.NEGATIVE_BUTTON_DISABLED == propertyKey) {
            view.setButtonEnabled(ModalDialogProperties.ButtonType.NEGATIVE,
                    !model.get(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED));
        } else if (ModalDialogProperties.TITLE_SCROLLABLE == propertyKey) {
            view.setTitleScrollable(model.get(ModalDialogProperties.TITLE_SCROLLABLE));
        } else if (ModalDialogProperties.CONTROLLER == propertyKey) {
            view.setOnButtonClickedCallback((buttonType) -> {
                model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
            });
        } else if (ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE == propertyKey) {
            // Intentionally left empty since this is a property for the dialog container.
        } else if (ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY == propertyKey) {
            assert checkFilterTouchConsistency(model);
            view.setFilterTouchForSecurity(
                    model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY));
        } else if (ModalDialogProperties.CONTENT_DESCRIPTION == propertyKey) {
            // Intentionally left empty since this is a property used for the dialog container.
        } else if (ModalDialogProperties.PRIMARY_BUTTON_FILLED == propertyKey) {
            assert checkFilledButtonConsistency(model);
            // Intentionally left empty since this is only read once before the dialog is inflated.
        } else {
            assert false : "Unhandled property detected in ModalDialogViewBinder!";
        }
    }

    /**
     * Checks if FILTER_TOUCH_FOR_SECURITY flag is consistent with the set of enabled buttons.
     * Touch event filtering in ModalDialogView is only applied to standard buttons. When buttons
     * are hidden, filtering touch events doesn't have effect.
     * @return false if security sensitive dialog doesn't have standard buttons.
     */
    private static boolean checkFilterTouchConsistency(PropertyModel model) {
        return !model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY)
                || !TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT))
                || !TextUtils.isEmpty(model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }

    /**
     * Checks if the PRIMARY_BUTTON_FILLED property is consistent with the set of enabled buttons.
     * The primary button cannot be the only button in the dialog.
     * @return false if the property is set to true, but there is only one button.
     */
    private static boolean checkFilledButtonConsistency(PropertyModel model) {
        return !(model.get(ModalDialogProperties.PRIMARY_BUTTON_FILLED)
                && (TextUtils.isEmpty(model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT))
                        || TextUtils.isEmpty(
                                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT))));
    }
}
