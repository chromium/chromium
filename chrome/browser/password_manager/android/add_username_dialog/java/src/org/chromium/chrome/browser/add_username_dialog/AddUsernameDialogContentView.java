// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.Callback;
import org.chromium.ui.text.EmptyTextWatcher;

public class AddUsernameDialogContentView extends LinearLayout {
    private Callback<String> mUsernameChangedCallback;

    public AddUsernameDialogContentView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        TextInputEditText usernameInput = findViewById(R.id.username);
        usernameInput.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence username, int i, int i1, int i2) {
                        assert mUsernameChangedCallback != null;
                        mUsernameChangedCallback.onResult(username.toString());
                    }
                });
        usernameInput.requestFocus();
    }

    void setPassword(String password) {
        TextInputEditText passwordInput = findViewById(R.id.password);
        passwordInput.setText(password);
    }

    void setUsernameChangedCallback(Callback<String> callback) {
        mUsernameChangedCallback = callback;
    }
}
