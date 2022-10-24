// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * View class for password save/update dialog.
 * Depending on whether password-edit-dialog-with-details flag is on, the views look different,
 * and this is reflected in this view inheritors {@link UsernameSelectionConfirmationView}
 * and {@link PasswordEditDialogWithDetailsView}
 */
abstract class PasswordEditDialogView extends LinearLayout {
    private TextView mFooterView;

    public PasswordEditDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mFooterView = findViewById(R.id.footer);
    }

    void setFooter(String footer) {
        mFooterView.setVisibility(!TextUtils.isEmpty(footer) ? View.VISIBLE : View.GONE);
        mFooterView.setText(footer);
    }

    /**
     * Sets initial password
     * Note: override this in the inheritor if password needs to be displayed
     */
    void setPassword(String password) {}

    /**
     * Sets callback for handling password change
     * Note: override this in the inheritor if password change needs to be handled
     */
    void setPasswordChangedCallback(Callback<String> callback) {}

    /**
     * Sets password error to be displayed in UI.
     * Setting null error means that no error will be displayed.
     * Note: override this in the inheritor if setting password error is needed
     *
     * @param error Error text
     */
    void setPasswordError(String error) {}
}
