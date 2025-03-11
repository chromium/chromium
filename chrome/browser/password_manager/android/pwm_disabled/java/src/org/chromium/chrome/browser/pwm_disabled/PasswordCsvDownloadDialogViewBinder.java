// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.DETAILS_PARAGRAPH1;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.DETAILS_PARAGRAPH2;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.EXPORT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.TITLE;

import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link PasswordCsvDownloadDialogProperties} changes in a {@link PropertyModel} to suitable
 * methods in {@link PasswordCsvDownloadDialogFragment}'s content view.
 */
@NullMarked
class PasswordCsvDownloadDialogViewBinder {
    private PasswordCsvDownloadDialogViewBinder() {}

    static void bind(PropertyModel model, View dialogView, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            ((TextView) dialogView.findViewById(R.id.title)).setText(model.get(TITLE));
        } else if (propertyKey == DETAILS_PARAGRAPH1) {
            TextView detailsParagraph1 = dialogView.findViewById(R.id.details_paragraph1);
            detailsParagraph1.setText(model.get(DETAILS_PARAGRAPH1));
            detailsParagraph1.setMovementMethod(LinkMovementMethod.getInstance());
        } else if (propertyKey == DETAILS_PARAGRAPH2) {
            ((TextView) dialogView.findViewById(R.id.details_paragraph2))
                    .setText(model.get(DETAILS_PARAGRAPH2));
        } else if (propertyKey == EXPORT_BUTTON_CALLBACK) {
            dialogView
                    .findViewById(R.id.positive_button)
                    .setOnClickListener(v -> model.get(EXPORT_BUTTON_CALLBACK).run());
        } else if (propertyKey == CLOSE_BUTTON_CALLBACK) {
            dialogView
                    .findViewById(R.id.negative_button)
                    .setOnClickListener(v -> model.get(CLOSE_BUTTON_CALLBACK).run());
        } else {
            assert false : "Property " + propertyKey.toString() + " not handler in the binder";
        }
    }
}
