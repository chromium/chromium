// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.os.Bundle;
import android.text.InputType;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageButton;
import androidx.fragment.app.Fragment;
import com.google.android.material.textfield.TextInputLayout;
import org.chromium.chrome.R;
import org.chromium.ui.widget.Toast;

/**
 * Password entry editor that allows editing passwords stored in Chrome.
 */
public class PasswordEntryEditor extends Fragment {
    static final String VIEW_BUTTON_PRESSED = "viewButtonPressed";
    public static final String CREDENTIAL_URL = "credentialUrl";
    public static final String CREDENTIAL_NAME = "credentialName";
    public static final String CREDENTIAL_PASSWORD = "credentialPassword";

    private EditText mSiteText;
    private EditText mUsernameText;
    private EditText mPasswordText;
    private TextInputLayout mPasswordLabel;

    private ImageButton mViewPasswordButton;

    private Runnable mPendingAction;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
        return inflater.inflate(R.layout.password_entry_editor, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mSiteText = (EditText) view.findViewById(R.id.site_edit);
        mUsernameText = (EditText) view.findViewById(R.id.username_edit);
        mPasswordText = (EditText) view.findViewById(R.id.password_edit);
        mPasswordLabel = (TextInputLayout) view.findViewById(R.id.password_label);
        mViewPasswordButton = view.findViewById(R.id.password_entry_editor_view_password);

        mSiteText.setText(getArguments().getString(CREDENTIAL_URL));
        mUsernameText.setText(getArguments().getString(CREDENTIAL_NAME));
        mPasswordText.setText(getArguments().getString(CREDENTIAL_PASSWORD));
        maskPassword();
    }

    @Override
    public void onResume() {
        super.onResume();
        boolean hasValidReauth = ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        if (!hasValidReauth) {
            maskPassword();
        } else if (mPendingAction != null) {
            mPendingAction.run();
        }
        mPendingAction = null;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        inflater.inflate(R.menu.password_entry_editor_action_bar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_save_edited_password) {
            saveChanges();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void maskPassword() {
        getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        mPasswordText.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mViewPasswordButton.setImageResource(R.drawable.ic_visibility_black);
        mViewPasswordButton.setOnClickListener(
                (unusedView)
                        -> this.performActionAfterReauth(this::unmaskPassword,
                                R.string.lockscreen_description_view,
                                R.string.password_entry_view_set_screen_lock));
    }

    private void unmaskPassword() {
        getActivity().getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
        mPasswordText.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mViewPasswordButton.setImageResource(R.drawable.ic_visibility_off_black);
        mViewPasswordButton.setOnClickListener((unusedView) -> this.maskPassword());
    }

    private void saveChanges() {
        String password = mPasswordText.getText().toString();
        if (TextUtils.isEmpty(password)) {
            mPasswordLabel.setError(getContext().getString(
                    R.string.pref_edit_dialog_field_required_validation_message));
        } else {
            PasswordEditingDelegateProvider.getInstance()
                    .getPasswordEditingDelegate()
                    .editSavedPasswordEntry(
                            mUsernameText.getText().toString(), mPasswordText.getText().toString());
            getActivity().finish();
        }
    }

    private void performActionAfterReauth(
            Runnable action, int reasonString, int noScreenLockMessage) {
        if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
            Toast.makeText(getActivity().getApplicationContext(), noScreenLockMessage,
                         Toast.LENGTH_LONG)
                    .show();
            return;
        }
        if (ReauthenticationManager.authenticationStillValid(
                    ReauthenticationManager.ReauthScope.ONE_AT_A_TIME)) {
            action.run();
            return;
        }
        mPendingAction = action;
        ReauthenticationManager.displayReauthenticationFragment(reasonString, View.NO_ID,
                getParentFragmentManager(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        PasswordEditingDelegateProvider.getInstance().getPasswordEditingDelegate().destroy();
    }
}
