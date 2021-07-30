// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.frebottomgroup;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.ui.R;
import org.chromium.chrome.browser.signin.ui.account_picker.ExistingAccountRowViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Stateless FREBottomGroup view binder.
 */
class FREBottomGroupViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == FREBottomGroupProperties.ON_CONTINUE_AS_CLICKED) {
            view.findViewById(R.id.signin_fre_continue_button)
                    .setOnClickListener(model.get(FREBottomGroupProperties.ON_CONTINUE_AS_CLICKED));
        } else if (propertyKey == FREBottomGroupProperties.ON_DISMISS_CLICKED) {
            view.findViewById(R.id.signin_fre_dismiss_button)
                    .setOnClickListener(model.get(FREBottomGroupProperties.ON_DISMISS_CLICKED));
        } else if (propertyKey == FREBottomGroupProperties.ON_SELECTED_ACCOUNT_CLICKED) {
            view.findViewById(R.id.signin_fre_selected_account)
                    .setOnClickListener(
                            model.get(FREBottomGroupProperties.ON_SELECTED_ACCOUNT_CLICKED));
        } else if (propertyKey == FREBottomGroupProperties.SELECTED_ACCOUNT_DATA) {
            final @Nullable DisplayableProfileData profileData =
                    model.get(FREBottomGroupProperties.SELECTED_ACCOUNT_DATA);
            if (profileData == null) {
                view.findViewById(R.id.signin_fre_selected_account).setVisibility(View.INVISIBLE);
                ButtonCompat button = view.findViewById(R.id.signin_fre_continue_button);
                button.setText(R.string.signin_add_account_to_device);
            } else {
                ExistingAccountRowViewBinder.bindAccountView(
                        profileData, view.findViewById(R.id.signin_fre_selected_account));
                view.findViewById(R.id.signin_fre_selected_account).setVisibility(View.VISIBLE);
                ButtonCompat button = view.findViewById(R.id.signin_fre_continue_button);
                button.setText(view.getContext().getString(R.string.signin_promo_continue_as,
                        profileData.getGivenNameOrFullNameOrEmail()));
            }
        } else {
            throw new IllegalArgumentException("Unknown property key:" + propertyKey);
        }
    }

    private FREBottomGroupViewBinder() {}
}
