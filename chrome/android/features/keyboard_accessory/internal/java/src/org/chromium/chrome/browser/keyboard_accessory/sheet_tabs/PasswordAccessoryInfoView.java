// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * This view represents a section of user credentials in the password tab of the keyboard accessory.
 */
class PasswordAccessoryInfoView extends LinearLayout {
    private TextView mTitle;
    private ImageView mIcon;
    private ChipView mUsername;
    private ChipView mPassword;

    /**
     * Constructor for inflating from XML.
     */
    public PasswordAccessoryInfoView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitle = findViewById(R.id.password_info_title);
        mIcon = findViewById(R.id.favicon);
        mUsername = findViewById(R.id.suggestion_text);
        mPassword = findViewById(R.id.password_text);
    }

    void setIconForBitmap(@Nullable Drawable icon) {
        final int kIconSize = getContext().getResources().getDimensionPixelSize(
                R.dimen.keyboard_accessory_suggestion_icon_size);
        if (icon != null) icon.setBounds(0, 0, kIconSize, kIconSize);
        mIcon.setImageDrawable(icon);
    }

    TextView getTitle() {
        return mTitle;
    }

    ChipView getUsername() {
        return mUsername;
    }

    ChipView getPassword() {
        return mPassword;
    }

    static void showWarningDialog(Context context) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());

        View dialogBody = inflater.inflate(R.layout.confirmation_dialog_view, null);

        TextView titleView = dialogBody.findViewById(R.id.confirmation_dialog_title);
        titleView.setText(R.string.passwords_not_secure_filling);

        TextView messageView = dialogBody.findViewById(R.id.confirmation_dialog_message);
        messageView.setText(R.string.passwords_not_secure_filling_details);

        builder.setView(dialogBody).setPositiveButton(R.string.ok, null).create().show();
    };
}
