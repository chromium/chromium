// Copyright 2021 The Chromium Authors. All rights reserved.
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
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import com.google.android.material.textfield.TextInputEditText;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The custom view for password edit modal dialog.
 */
public class PasswordEditDialogView extends LinearLayout implements OnItemSelectedListener {
    private Spinner mUsernamesSpinner;
    private TextView mFooterView;
    private Callback<Integer> mUsernameSelectedCallback;
    private TextInputEditText mPasswordView;
    private Callback<String> mPasswordChangedCallback;

    /**
     * The view binder method to propagate parameters from model to view
     */
    public static void bind(
            PropertyModel model, PasswordEditDialogView dialogView, PropertyKey propertyKey) {
        if (propertyKey == PasswordEditDialogProperties.USERNAMES) {
            dialogView.setUsernames(model.get(PasswordEditDialogProperties.USERNAMES),
                    model.get(PasswordEditDialogProperties.SELECTED_USERNAME_INDEX));
        } else if (propertyKey == PasswordEditDialogProperties.SELECTED_USERNAME_INDEX) {
            // Propagation of USERNAMES property triggers passing both USERNAMES and
            // SELECTED_USERNAME_INDEX properties to the view. This is safe because both properties
            // are set through property model builder and available by the time the property model
            // is bound to the view. The SELECTED_USERNAME_INDEX property is writable since it
            // maintains username index of the user, currently selected in UI. Updating the property
            // by itself doesn't get propagated to the view as the value originates in the view and
            // gets routed to coordinator through USERNAME_SELECTED_CALLBACK.
        } else if (propertyKey == PasswordEditDialogProperties.FOOTER) {
            dialogView.setFooter(model.get(PasswordEditDialogProperties.FOOTER));
        } else if (propertyKey == PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK) {
            dialogView.setUsernameSelectedCallback(
                    model.get(PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK) {
            dialogView.setPasswordChangedCallback(
                    model.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK));
        } else if (propertyKey == PasswordEditDialogProperties.PASSWORD) {
            dialogView.setPassword(model.get(PasswordEditDialogProperties.PASSWORD));
        } else if (propertyKey == PasswordEditDialogProperties.EMPTY_PASSWORD_ERROR) {
            // TODO(crbug.com/1315916): Handle displaying empty password error in the following CLs
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }

    public PasswordEditDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Stores references to the dialog fields after dialog inflation.
     */
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mUsernamesSpinner = findViewById(R.id.usernames_spinner);
        mUsernamesSpinner.setOnItemSelectedListener(this);

        mFooterView = findViewById(R.id.footer);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)) {
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
    }

    void setUsernames(List<String> usernames, int selectedUsernameIndex) {
        ArrayAdapter<String> usernamesAdapter =
                new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_item);
        usernamesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        usernamesAdapter.addAll(usernames);
        mUsernamesSpinner.setAdapter(usernamesAdapter);
        mUsernamesSpinner.setSelection(selectedUsernameIndex);
    }

    void setPassword(String password) {
        if (mPasswordView == null || mPasswordView.getText().toString().equals(password)) return;
        mPasswordView.setText(password);
    }

    void setFooter(String footer) {
        mFooterView.setVisibility(!TextUtils.isEmpty(footer) ? View.VISIBLE : View.GONE);
        mFooterView.setText(footer);
    }

    void setUsernameSelectedCallback(Callback<Integer> callback) {
        mUsernameSelectedCallback = callback;
    }

    void setPasswordChangedCallback(Callback<String> callback) {
        mPasswordChangedCallback = callback;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (mUsernameSelectedCallback != null) {
            mUsernameSelectedCallback.onResult(position);
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}
}
