// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.AddAccountRowProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the buildView and bindView util methods of the
 * add account row.
 */
class AddAccountRowViewBinder {
    private AddAccountRowViewBinder() {}

    static View buildView(ViewGroup parent) {
        @LayoutRes
        int layoutRes = ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                ? R.layout.account_picker_new_account_row
                : R.layout.account_picker_new_account_row_legacy;
        return LayoutInflater.from(parent.getContext()).inflate(layoutRes, parent, false);
    }

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AddAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(model.get(AddAccountRowProperties.ON_CLICK_LISTENER));
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }
}
