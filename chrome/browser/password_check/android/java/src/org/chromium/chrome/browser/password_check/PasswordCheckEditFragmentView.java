// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager.ReauthScope;

/**
 * This class is responsible for rendering the edit fragment where users can provide a new password
 * for compromised credentials.
 * TODO(crbug.com/1092444): Make this a component that is reusable in password settings as well.
 */
public class PasswordCheckEditFragmentView extends PreferenceFragmentCompat {
    public static final String EXTRA_COMPROMISED_CREDENTIAL = "extra_compromised_credential";
    @VisibleForTesting
    static final String EXTRA_NEW_PASSWORD = "extra_new_password";
    static final String EXTRA_PASSWORD_VISIBLE = "extra_password_visible";

    private Supplier<PasswordCheck> mPasswordCheckFactory;
    private String mNewPassword;
    private CompromisedCredential mCredential;
    private boolean mPasswordVisible;

    private EditText mPasswordText;
    private MenuItem mSaveButton;
    private TextInputLayout mPasswordLabel;
    private ImageButton mViewPasswordButton;
    private ImageButton mMaskPasswordButton;

    /**
     * Initializes the password check factory that allows to retrieve a {@link PasswordCheck}
     * implementation used for saving the changed credential.
     * @param passwordCheckFactory A {@link Supplier<PasswordCheck>}.
     */
    public void setCheckProvider(Supplier<PasswordCheck> passwordCheckFactory) {
        mPasswordCheckFactory = passwordCheckFactory;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {}

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
        return inflater.inflate(R.layout.password_check_edit_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mCredential = getCredentialFromInstanceStateOrLaunchBundle(savedInstanceState);
        mNewPassword = getNewPasswordFromInstanceStateOrLaunchBundle(savedInstanceState);
        mPasswordVisible = getPasswordVisibleFromInstanceState(savedInstanceState);

        TextView hintText = view.findViewById(R.id.edit_hint);
        hintText.setText(getString(R.string.password_edit_hint, mCredential.getDisplayOrigin()));

        EditText siteText = (EditText) view.findViewById(R.id.site_edit);
        siteText.setText(mCredential.getDisplayOrigin());

        EditText usernameText = (EditText) view.findViewById(R.id.username_edit);
        usernameText.setText(mCredential.getDisplayUsername());

        mPasswordLabel = (TextInputLayout) view.findViewById(R.id.password_label);
        mPasswordText = (EditText) view.findViewById(R.id.password_edit);
        mPasswordText.setText(mCredential.getPassword());
        mPasswordText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable editable) {
                mNewPassword = mPasswordText.getText().toString();
                checkSavingConditions(TextUtils.isEmpty(mNewPassword));
            }
        });
        // Enforce that even the initial password (maybe from a saved instance) cannot be empty.
        checkSavingConditions(TextUtils.isEmpty(mNewPassword));

        mViewPasswordButton = view.findViewById(R.id.password_entry_editor_view_password);
        mViewPasswordButton.setOnClickListener(unusedView -> this.unmaskPassword());
        mMaskPasswordButton = view.findViewById(R.id.password_entry_editor_mask_password);
        mMaskPasswordButton.setOnClickListener(unusedView -> this.maskPassword());
        if (mPasswordVisible) {
            unmaskPassword();
        } else {
            maskPassword();
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear(); // Remove help and feedback for this screen.
        inflater.inflate(R.menu.password_check_editor_action_bar_menu, menu);
        mSaveButton = menu.findItem(R.id.action_save_edited_password);
        checkSavingConditions(mNewPassword.isEmpty()); // Enable the newly created save button.
        // TODO(crbug.com/1092444): Make the back arrow an 'X'.
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!ReauthenticationManager.authenticationStillValid(ReauthScope.ONE_AT_A_TIME)) {
            // If the page was idle (e.g. screenlocked for a few minutes), go back to the previous
            // page to ensure the user goes through reauth again.
            // TODO(crbug.com/1114720): Trigger another reauth instead.
            getActivity().finish();
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putParcelable(EXTRA_COMPROMISED_CREDENTIAL, mCredential);
        outState.putString(EXTRA_NEW_PASSWORD, mNewPassword);
        outState.putBoolean(EXTRA_PASSWORD_VISIBLE, mPasswordVisible);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_save_edited_password) {
            PasswordCheckMetricsRecorder.recordUiUserAction(
                    PasswordCheckUserAction.EDITED_PASSWORD);
            PasswordCheckMetricsRecorder.recordCheckResolutionAction(
                    PasswordCheckResolutionAction.EDITED_PASSWORD, mCredential);
            saveChanges();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void saveChanges() {
        assert !TextUtils.isEmpty(mNewPassword);
        mPasswordCheckFactory.get().updateCredential(mCredential, mNewPassword);
        getActivity().finish();
    }

    private CompromisedCredential getCredentialFromInstanceStateOrLaunchBundle(
            Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(EXTRA_COMPROMISED_CREDENTIAL)) {
            return savedInstanceState.getParcelable(EXTRA_COMPROMISED_CREDENTIAL);
        }
        Bundle extras = getArguments();
        assert extras != null
                && extras.containsKey(EXTRA_COMPROMISED_CREDENTIAL)
            : "PasswordCheckEditFragmentView must be launched with a compromised credential "
                        + "extra!";
        return extras.getParcelable(EXTRA_COMPROMISED_CREDENTIAL);
    }

    private String getNewPasswordFromInstanceStateOrLaunchBundle(Bundle savedInstanceState) {
        if (savedInstanceState != null && savedInstanceState.containsKey(EXTRA_NEW_PASSWORD)) {
            return savedInstanceState.getParcelable(EXTRA_NEW_PASSWORD);
        }
        Bundle extras = getArguments();
        if (extras != null && extras.containsKey(EXTRA_NEW_PASSWORD)) {
            return extras.getParcelable(EXTRA_NEW_PASSWORD);
        }
        return mCredential.getPassword();
    }

    private void checkSavingConditions(boolean emptyPassword) {
        if (mSaveButton != null) mSaveButton.setEnabled(!emptyPassword);
        mPasswordLabel.setError(emptyPassword ? getContext().getString(
                                        R.string.pref_edit_dialog_field_required_validation_message)
                                              : "");
    }

    private boolean getPasswordVisibleFromInstanceState(Bundle savedInstanceState) {
        return (savedInstanceState != null
                       && savedInstanceState.containsKey(EXTRA_PASSWORD_VISIBLE))
                && savedInstanceState.getBoolean(EXTRA_PASSWORD_VISIBLE);
    }

    private void maskPassword() {
        getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        mPasswordText.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mViewPasswordButton.setVisibility(View.VISIBLE);
        mMaskPasswordButton.setVisibility(View.GONE);
        mPasswordVisible = false;
    }

    private void unmaskPassword() {
        getActivity().getWindow().setFlags(
                WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
        mPasswordText.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        mViewPasswordButton.setVisibility(View.GONE);
        mMaskPasswordButton.setVisibility(View.VISIBLE);
        mPasswordVisible = true;
    }
}
