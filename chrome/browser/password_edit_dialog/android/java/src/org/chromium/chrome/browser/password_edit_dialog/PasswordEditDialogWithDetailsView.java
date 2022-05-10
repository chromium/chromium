// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;

import java.util.List;

/**
 * The view which represents a username label and input control and
 * a password label and input control.
 * The view has the functionality of editing both password and username.
 */
public class PasswordEditDialogWithDetailsView extends PasswordEditDialogView {
    private AutoCompleteTextView mUsernameView;
    private TextInputEditText mPasswordView;
    private Callback<String> mUsernameChangedCallback;
    private Callback<String> mPasswordChangedCallback;

    public PasswordEditDialogWithDetailsView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Stores references to the dialog fields after dialog inflation.
     */
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mUsernameView = findViewById(R.id.username_view);
        mUsernameView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                if (mUsernameChangedCallback == null) return;
                mUsernameChangedCallback.onResult(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {}
        });
        TextInputLayout usernameInput = findViewById(R.id.username_input_layout);
        usernameInput.setEndIconOnClickListener(view -> mUsernameView.showDropDown());

        mPasswordView = findViewById(R.id.password);
        mPasswordView.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mPasswordView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                if (mPasswordChangedCallback == null) return;
                mPasswordChangedCallback.onResult(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {}
        });
    }

    @Override
    public void setUsernames(List<String> usernames, String initialUsername) {
        ArrayAdapter<String> usernamesAdapter = new NoFilterArrayAdapter<>(
                getContext(), android.R.layout.simple_spinner_item, usernames);
        usernamesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mUsernameView.setAdapter(usernamesAdapter);
        mUsernameView.setText(initialUsername);
    }

    @Override
    public void setUsernameChangedCallback(Callback<String> callback) {
        mUsernameChangedCallback = callback;
    }

    @Override
    public void setPassword(String password) {
        if (mPasswordView.getText().toString().equals(password)) return;
        mPasswordView.setText(password);
    }

    @Override
    public void setPasswordChangedCallback(Callback<String> callback) {
        mPasswordChangedCallback = callback;
    }
}
