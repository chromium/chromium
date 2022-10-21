// Copyright 2022 The Chromium Authors
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
class UsernameSelectionConfirmationView
        extends PasswordEditDialogView implements OnItemSelectedListener {
    private Spinner mUsernamesSpinner;
    private Callback<Integer> mUsernameSelectedCallback;

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
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (mUsernameSelectedCallback != null) {
            mUsernameSelectedCallback.onResult(position);
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}

    /**
     * Sets callback for handling username change
     *
     * @param callback The callback to be called with new index of the selected username
     */
    public void setUsernameChangedCallback(Callback<Integer> callback) {
        mUsernameSelectedCallback = callback;
    }

    /**
     * Sets list of known usernames which can be selected from the list by user
     *
     * @param usernames Known usernames list
     * @param initialUsernameIndex Username that will be selected in the spinner
     */
    public void setUsernames(List<String> usernames, int initialUsernameIndex) {
        ArrayAdapter<String> usernamesAdapter =
                new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_item);
        usernamesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        usernamesAdapter.addAll(usernames);
        mUsernamesSpinner.setAdapter(usernamesAdapter);
        mUsernamesSpinner.setSelection(initialUsernameIndex);
    }
}
