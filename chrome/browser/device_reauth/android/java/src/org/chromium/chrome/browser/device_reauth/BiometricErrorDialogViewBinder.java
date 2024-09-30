// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.DESCRIPTION;
import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.MORE_DETAILS;
import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link BiometricErrorDialogProperties changes in a {@link PropertyModel} to
 * {@link BiometricErrorDialogController}'s modal dialog custom view.
 */
class BiometricErrorDialogViewBinder {
    private BiometricErrorDialogViewBinder() {}
    ;

    static void bind(PropertyModel model, View view, PropertyKey key) {
        if (key == TITLE) {
            ((TextView) view.findViewById(R.id.error_dialog_title)).setText(model.get(TITLE));
        } else if (key == DESCRIPTION) {
            ((TextView) view.findViewById(R.id.description)).setText(model.get(DESCRIPTION));
        } else if (key == MORE_DETAILS) {
            ((TextView) view.findViewById(R.id.more_details)).setText(model.get(MORE_DETAILS));
        } else {
            assert false : "Property " + key.toString() + " not handled in the binder";
        }
    }
}
