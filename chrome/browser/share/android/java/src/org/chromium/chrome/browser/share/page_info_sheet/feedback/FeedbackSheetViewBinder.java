// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet.feedback;

import android.view.View;
import android.widget.Button;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator.FeedbackOption;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

class FeedbackSheetViewBinder {

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey.equals(FeedbackSheetProperties.OPTION_SELECTED_CALLBACK)) {
            RadioButtonLayout radioButtonLayout = view.findViewById(R.id.radio_buttons);
            radioButtonLayout.setOnCheckedChangeListener(
                    model.get(FeedbackSheetProperties.OPTION_SELECTED_CALLBACK));
        } else if (propertyKey.equals(FeedbackSheetProperties.AVAILABLE_OPTIONS)) {
            RadioButtonLayout radioButtonLayout = view.findViewById(R.id.radio_buttons);
            List<FeedbackOption> keysWithLabels =
                    model.get(FeedbackSheetProperties.AVAILABLE_OPTIONS);
            List<CharSequence> uiStrings = new ArrayList<>();
            List<String> keys = new ArrayList<>();

            keysWithLabels.forEach(
                    (keyLabelPair) -> {
                        keys.add(keyLabelPair.optionKey);
                        uiStrings.add(view.getResources().getString(keyLabelPair.displayTextId));
                    });
            radioButtonLayout.addOptions(uiStrings, keys);
        } else if (propertyKey.equals(FeedbackSheetProperties.ON_ACCEPT_CLICKED)) {
            Button acceptButton = view.findViewById(R.id.accept_button);
            acceptButton.setOnClickListener(model.get(FeedbackSheetProperties.ON_ACCEPT_CLICKED));
        } else if (propertyKey.equals(FeedbackSheetProperties.ON_CANCEL_CLICKED)) {
            Button cancelButton = view.findViewById(R.id.cancel_button);
            cancelButton.setOnClickListener(model.get(FeedbackSheetProperties.ON_CANCEL_CLICKED));
        } else if (propertyKey.equals(FeedbackSheetProperties.IS_ACCEPT_BUTTON_ENABLED)) {
            Button acceptButton = view.findViewById(R.id.accept_button);
            acceptButton.setEnabled(model.get(FeedbackSheetProperties.IS_ACCEPT_BUTTON_ENABLED));
        }
    }
}
