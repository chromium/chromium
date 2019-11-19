// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.content.Context;
import android.support.v4.widget.TextViewCompat;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class creates the model for permission dialog.
 */
class PermissionDialogModel {
    public static PropertyModel getModel(
            ModalDialogProperties.Controller controller, PermissionDialogDelegate delegate) {
        Context context = delegate.getTab().getActivity();
        LayoutInflater inflater = LayoutInflater.from(context);
        View customView = inflater.inflate(R.layout.permission_dialog, null);

        String messageText = delegate.getMessageText();
        assert !TextUtils.isEmpty(messageText);

        TextView messageTextView = customView.findViewById(R.id.text);
        messageTextView.setText(messageText);
        TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                messageTextView, delegate.getDrawableId(), 0, 0, 0);

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        delegate.getPrimaryButtonText())
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        delegate.getSecondaryButtonText())
                .with(ModalDialogProperties.CONTENT_DESCRIPTION, delegate.getMessageText())
                .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true)
                .build();
    }
}
