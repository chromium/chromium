// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerProperties.ExistingAccountRowProperties;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the buildView and bindView util methods of the
 * existing account row.
 */
class ExistingAccountRowViewBinder {
    private ExistingAccountRowViewBinder() {}

    static View buildView(ViewGroup parent) {
        @LayoutRes
        int layoutRes = ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                ? R.layout.account_picker_row
                : R.layout.account_picker_row_legacy;
        return LayoutInflater.from(parent.getContext()).inflate(layoutRes, parent, false);
    }

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        DisplayableProfileData profileData = model.get(ExistingAccountRowProperties.PROFILE_DATA);
        if (propertyKey == ExistingAccountRowProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(v
                    -> model.get(ExistingAccountRowProperties.ON_CLICK_LISTENER)
                               .onResult(profileData));
        } else if (propertyKey == ExistingAccountRowProperties.PROFILE_DATA) {
            bindAccountView(profileData, view);
        } else if (propertyKey == ExistingAccountRowProperties.IS_SELECTED_ACCOUNT) {
            ImageView selectionMark = view.findViewById(R.id.account_selection_mark);
            if (model.get(ExistingAccountRowProperties.IS_SELECTED_ACCOUNT)) {
                selectionMark.setImageResource(R.drawable.ic_check_googblue_24dp);
                ImageViewCompat.setImageTintList(selectionMark,
                        ContextCompat.getColorStateList(
                                view.getContext(), R.color.default_icon_color_blue));
                selectionMark.setVisibility(View.VISIBLE);
            } else {
                selectionMark.setVisibility(View.GONE);
            }
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }

    /**
     * Binds the view with the given profile data.
     *
     * @param profileData profile data needs to bind.
     * @param view A view object inflated from @layout/account_picker_row.
     */
    static void bindAccountView(DisplayableProfileData profileData, View view) {
        ImageView accountImage = view.findViewById(R.id.account_image);
        accountImage.setImageDrawable(profileData.getImage());

        TextView accountTextPrimary = view.findViewById(R.id.account_text_primary);
        TextView accountTextSecondary = view.findViewById(R.id.account_text_secondary);

        String fullName = profileData.getFullName();
        if (!TextUtils.isEmpty(fullName)) {
            accountTextPrimary.setText(fullName);
            accountTextSecondary.setText(profileData.getAccountEmail());
            accountTextSecondary.setVisibility(View.VISIBLE);
        } else {
            // Full name is not available, show the email in the primary TextView.
            accountTextPrimary.setText(profileData.getAccountEmail());
            accountTextSecondary.setVisibility(View.GONE);
        }
    }
}
