// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.List;

/**
 * The view which represents a username label and input control and a password label and input
 * control. The view has the functionality of editing both password and username.
 */
class PasswordEditDialogView extends LinearLayout {
    private AutoCompleteTextView mUsernameView;
    private TextInputLayout mUsernameInputLayout;
    private TextInputEditText mPasswordField;
    private TextInputLayout mPasswordInputLayout;
    private Callback<String> mUsernameChangedCallback;
    private Callback<String> mPasswordChangedCallback;
    private List<String> mUsernames;
    private TextView mFooterView;

    public PasswordEditDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Stores references to the dialog fields after dialog inflation. */
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mFooterView = findViewById(R.id.footer);
        mUsernameView = findViewById(R.id.username_view);
        mUsernameInputLayout = findViewById(R.id.username_input_layout);
        mUsernameInputLayout.setEndIconOnClickListener(view -> mUsernameView.showDropDown());
        mUsernameView.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        if (mUsernameChangedCallback == null) return;
                        mUsernameChangedCallback.onResult(charSequence.toString());
                    }

                    @Override
                    public void afterTextChanged(Editable editable) {
                        setDropDownVisibility(editable.toString());
                    }
                });

        mPasswordField = findViewById(R.id.password);
        mPasswordField.setInputType(
                InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_VARIATION_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mPasswordField.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence charSequence, int i, int i1, int i2) {}

                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        if (mPasswordChangedCallback == null) return;
                        mPasswordChangedCallback.onResult(charSequence.toString());
                    }

                    @Override
                    public void afterTextChanged(Editable editable) {}
                });
        mPasswordInputLayout = findViewById(R.id.password_text_input_layout);
    }

    public void setPassword(String password) {
        if (mPasswordField.getText().toString().equals(password)) return;
        mPasswordField.setText(password);
    }

    public void setPasswordChangedCallback(Callback<String> callback) {
        mPasswordChangedCallback = callback;
    }

    public void setPasswordError(String error) {
        mPasswordInputLayout.setError(error);
    }

    /** Sets usernames list in the AutoCompleteTextView */
    public void setUsernames(List<String> usernames) {
        mUsernames = usernames;
        ArrayAdapter<String> usernamesAdapter =
                new NoFilterArrayAdapter<>(
                        getContext(), R.layout.password_edit_dialog_dropdown_item, usernames);
        mUsernameView.setAdapter(usernamesAdapter);
        setDropDownVisibility(mUsernameView.getText().toString());
    }

    /**
     * Sets callback for handling username change.
     *
     * @param callback The callback to be called with new username typed into the
     *     AutoCompleteTextView
     */
    public void setUsernameChangedCallback(Callback<String> callback) {
        mUsernameChangedCallback = callback;
    }

    /** Sets username in the text input */
    public void setUsername(String username) {
        if (mUsernameView.getText().toString().equals(username)) return;
        mUsernameView.setText(username);
        setDropDownVisibility(username);
    }

    private void setDropDownVisibility(String currentUsername) {
        if (shouldShowDropDown(currentUsername)) {
            mUsernameInputLayout.setEndIconVisible(true);
        } else {
            // Hide the dropdown button and dismiss the dropdown (in case if it's open).
            mUsernameInputLayout.setEndIconVisible(false);
            mUsernameView.dismissDropDown();
        }
    }

    private boolean shouldShowDropDown(String currentUsername) {
        // Do not show the dropdown, when there are no usernames to list.
        if (mUsernames == null) return false;
        // Show the dropdown when there is more than one choice.
        if (mUsernames.size() > 1) return true;
        // Show the dropdown when there is one choice which is different from current username.
        return (mUsernames.size() == 1 && !mUsernames.get(0).equals(currentUsername));
    }

    void setFooter(String footer) {
        mFooterView.setVisibility(!TextUtils.isEmpty(footer) ? View.VISIBLE : View.GONE);
        mFooterView.setText(footer);
    }
}
