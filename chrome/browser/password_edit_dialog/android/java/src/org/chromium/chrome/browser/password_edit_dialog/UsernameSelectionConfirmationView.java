// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import org.chromium.base.Callback;

import java.util.List;

/**
 * The custom view for password edit modal dialog.
 */
public class UsernameSelectionConfirmationView
        extends PasswordEditDialogView implements OnItemSelectedListener {
    private Spinner mUsernamesSpinner;
    private Callback<String> mUsernameSelectedCallback;

    public UsernameSelectionConfirmationView(Context context, AttributeSet attrs) {
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
    }

    @Override
    public void setUsernames(List<String> usernames, String initialUsername) {
        ArrayAdapter<String> usernamesAdapter =
                new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_item);
        usernamesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        usernamesAdapter.addAll(usernames);
        mUsernamesSpinner.setAdapter(usernamesAdapter);

        int initialUsernameIndex = usernames.indexOf(initialUsername);
        assert initialUsernameIndex >= 0
            : "Initial username should be present in all usernames list";
        mUsernamesSpinner.setSelection(initialUsernameIndex);
    }

    @Override
    public void setUsernameChangedCallback(Callback<String> callback) {
        mUsernameSelectedCallback = callback;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (mUsernameSelectedCallback != null) {
            String username = mUsernamesSpinner.getItemAtPosition(position).toString();
            mUsernameSelectedCallback.onResult(username);
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}
}
