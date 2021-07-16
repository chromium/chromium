// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.frebottomgroup;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class FREBottomGroupProperties {
    static final ReadableObjectPropertyKey<OnClickListener> ON_SELECTED_ACCOUNT_CLICKED =
            new ReadableObjectPropertyKey<>("on_selected_account_clicked");
    static final WritableObjectPropertyKey<DisplayableProfileData> SELECTED_ACCOUNT_DATA =
            new WritableObjectPropertyKey<>("selected_account_data");

    // PropertyKey for the button |Continue as ...|
    static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_AS_CLICKED =
            new ReadableObjectPropertyKey<>("on_continue_as_clicked");

    // PropertyKey for the dismiss button
    static final ReadableObjectPropertyKey<OnClickListener> ON_DISMISS_CLICKED =
            new ReadableObjectPropertyKey<>("on_dismiss_clicked");

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ON_SELECTED_ACCOUNT_CLICKED,
            SELECTED_ACCOUNT_DATA, ON_CONTINUE_AS_CLICKED, ON_DISMISS_CLICKED};

    /**
     * Creates a default model for FRE bottom group.
     */
    static PropertyModel createModel(Runnable onSelectedAccountClicked,
            Runnable onContinueAsClicked, Runnable onDismissClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_SELECTED_ACCOUNT_CLICKED, v -> onSelectedAccountClicked.run())
                .with(SELECTED_ACCOUNT_DATA, null)
                .with(ON_CONTINUE_AS_CLICKED, v -> onContinueAsClicked.run())
                .with(ON_DISMISS_CLICKED, v -> onDismissClicked.run())
                .build();
    }

    private FREBottomGroupProperties() {}
}
