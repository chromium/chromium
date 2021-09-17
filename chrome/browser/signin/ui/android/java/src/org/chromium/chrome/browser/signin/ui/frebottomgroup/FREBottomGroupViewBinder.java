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
 * TODO(crbug/1248083): Create a customised view class for the view here
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
                ButtonCompat button = view.findViewById(R.id.signin_fre_continue_button);
                button.setText(R.string.signin_add_account_to_device);
            } else {
                ExistingAccountRowViewBinder.bindAccountView(
                        profileData, view.findViewById(R.id.signin_fre_selected_account));
                ButtonCompat button = view.findViewById(R.id.signin_fre_continue_button);
                button.setText(view.getContext().getString(R.string.signin_promo_continue_as,
                        profileData.getGivenNameOrFullNameOrEmail()));
            }
            updateVisibility(view, model);
        } else if (propertyKey == FREBottomGroupProperties.IS_SELECTED_ACCOUNT_SUPERVISED) {
            final boolean isSelectedAccountSupervised =
                    model.get(FREBottomGroupProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
            view.findViewById(R.id.signin_fre_selected_account)
                    .setEnabled(!isSelectedAccountSupervised);
            updateVisibility(view, model);
        } else if (propertyKey == FREBottomGroupProperties.ARE_NATIVE_AND_POLICY_LOADED) {
            updateVisibility(view, model);
        } else if (propertyKey == FREBottomGroupProperties.FRE_POLICY) {
            view.findViewById(R.id.fre_browser_managed_by_organization)
                    .setVisibility(model.get(FREBottomGroupProperties.FRE_POLICY) != null
                                    ? View.VISIBLE
                                    : View.GONE);
        } else {
            throw new IllegalArgumentException("Unknown property key:" + propertyKey);
        }
    }

    private static void updateVisibility(View view, PropertyModel model) {
        final boolean areNativeAndPolicyLoaded =
                model.get(FREBottomGroupProperties.ARE_NATIVE_AND_POLICY_LOADED);
        view.findViewById(R.id.signin_fre_progress_spinner)
                .setVisibility(areNativeAndPolicyLoaded ? View.GONE : View.VISIBLE);

        if (areNativeAndPolicyLoaded) {
            view.findViewById(R.id.signin_fre_selected_account)
                    .setVisibility(model.get(FREBottomGroupProperties.SELECTED_ACCOUNT_DATA) == null
                                    ? View.GONE
                                    : View.VISIBLE);
            view.findViewById(R.id.signin_fre_dismiss_button)
                    .setVisibility(
                            model.get(FREBottomGroupProperties.IS_SELECTED_ACCOUNT_SUPERVISED)
                                    ? View.GONE
                                    : View.VISIBLE);
        } else {
            view.findViewById(R.id.signin_fre_selected_account).setVisibility(View.GONE);
            view.findViewById(R.id.signin_fre_dismiss_button).setVisibility(View.GONE);
        }
        final int otherElementsVisibility = areNativeAndPolicyLoaded ? View.VISIBLE : View.GONE;
        view.findViewById(R.id.signin_fre_continue_button).setVisibility(otherElementsVisibility);
        view.findViewById(R.id.signin_fre_footer).setVisibility(otherElementsVisibility);
    }

    private FREBottomGroupViewBinder() {}
}
