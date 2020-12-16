// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.view.View;

import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the buildView and bindView util methods of the
 * add account row.
 */
class AddAccountRowViewBinder {
    private AddAccountRowViewBinder() {}

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AddAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(model.get(AddAccountRowProperties.ON_CLICK_LISTENER));
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }
}
