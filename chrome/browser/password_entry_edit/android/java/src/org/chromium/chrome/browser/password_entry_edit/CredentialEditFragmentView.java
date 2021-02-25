// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.content.Context;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.preference.PreferenceFragmentCompat;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * This class is responsible for rendering the edit fragment where users can edit a saved password.
 */
public class CredentialEditFragmentView extends PreferenceFragmentCompat {
    private ComponentStateDelegate mComponentStateDelegate;
    private TextInputLayout mUsernameInputLayout;
    private TextInputEditText mUsernameField;
    private TextInputLayout mPasswordInputLayout;
    private TextInputEditText mPasswordField;
    private ButtonCompat mDoneButton;

    interface UiActionHandler {
        /** Called when the user clicks the button to mask/unmask the password */
        void onMaskOrUnmaskPassword();

        /** Called when the text in the username field changes */
        void onUsernameTextChanged(String username);

        /** Called when the text in the password field changes */
        void onPasswordTextChanged(String password);

        /**
         * Called when the user clicks the button to copy the username
         *
         * @param context application context that can be used to get the {@link ClipboardManager}
         */
        void onCopyUsername(Context context);

        /**
         * Called when the user clicks the button to copy the password
         *
         * @param context application context that can be used to get the {@link ClipboardManager}
         */
        void onCopyPassword(Context context);

        /** Called when the user clicks the button to save the changes to the credential */
        void onSave();
    }

    // TODO(crbug.com/1178519): The coordinator should be made a LifecycleObserver instead.
    interface ComponentStateDelegate {
        /**
         * Called when the fragment is started.
         */
        void onStartFragment();

        /**
         * Called when the fragment is resumed.
         */
        void onResumeFragment();

        /**
         * Signals that the component is no longer needed.
         */
        void onDestroy();
    }

    /**
     * Sets the delegate that handles view events which affect the state of the component
     *
     * @param componentStateDelegate The delegate handling the view events.
     **/
    void setComponentStateDelegate(ComponentStateDelegate componentStateDelegate) {
        mComponentStateDelegate = componentStateDelegate;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        getActivity().setTitle(R.string.password_entry_viewer_edit_stored_password_action_title);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        return inflater.inflate(R.layout.credential_edit_view, container, false);
    }

    @Override
    public void onStart() {
        super.onStart();
        mUsernameInputLayout = getView().findViewById(R.id.username_text_input_layout);
        mUsernameField = getView().findViewById(R.id.username);
        View usernameIcon = getView().findViewById(R.id.copy_username_button);
        addLayoutChangeListener(mUsernameField, usernameIcon);

        mPasswordInputLayout = getView().findViewById(R.id.password_text_input_layout);
        mPasswordField = getView().findViewById(R.id.password);
        View passwordIcons = getView().findViewById(R.id.password_icons);
        addLayoutChangeListener(mPasswordField, passwordIcons);

        // TODO(crbug.com/1175785): Use this string for the deletion dialog body.
        getString(R.string.password_entry_edit_deletion_dialog_body);

        mDoneButton = getView().findViewById(R.id.button_primary);

        getView().findViewById(R.id.button_secondary).setOnClickListener((unusedView) -> dismiss());

        if (mComponentStateDelegate != null) mComponentStateDelegate.onStartFragment();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mComponentStateDelegate != null) mComponentStateDelegate.onResumeFragment();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (getActivity().isFinishing() && mComponentStateDelegate != null) {
            mComponentStateDelegate.onDestroy();
        }
    }

    void dismiss() {
        getActivity().finish();
    }

    void setUiActionHandler(UiActionHandler uiActionHandler) {
        ChromeImageButton usernameCopyButton = getView().findViewById(R.id.copy_username_button);
        usernameCopyButton.setOnClickListener(
                (unusedView)
                        -> uiActionHandler.onCopyUsername(getActivity().getApplicationContext()));

        ChromeImageButton passwordCopyButton = getView().findViewById(R.id.copy_password_button);
        passwordCopyButton.setOnClickListener(
                (unusedView)
                        -> uiActionHandler.onCopyPassword(getActivity().getApplicationContext()));

        ChromeImageButton passwordVisibilityButton =
                getView().findViewById(R.id.password_visibility_button);
        passwordVisibilityButton.setOnClickListener(
                (unusedView) -> uiActionHandler.onMaskOrUnmaskPassword());

        getView().findViewById(R.id.button_primary).setOnClickListener((unusedView) -> {
            uiActionHandler.onSave();
            dismiss();
        });

        getView().findViewById(R.id.button_secondary).setOnClickListener((unusedView) -> dismiss());

        mUsernameField.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                uiActionHandler.onUsernameTextChanged(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {}
        });

        mPasswordField.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

            @Override
            public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                uiActionHandler.onPasswordTextChanged(charSequence.toString());
            }

            @Override
            public void afterTextChanged(Editable editable) {}
        });
    }

    void setUrlOrApp(String urlOrApp) {
        TextView urlOrAppText = getView().findViewById(R.id.url_or_app);
        urlOrAppText.setText(urlOrApp);

        TextView editInfoText = getView().findViewById(R.id.edit_info);
        editInfoText.setText(getString(R.string.password_edit_hint, urlOrApp));
    }

    void setUsername(String username) {
        // Don't update the text field if it has the same contents, as this will reset the cursor
        // position to the beginning.
        if (mUsernameField.getText().toString().equals(username)) return;
        mUsernameField.setText(username);
    }

    void changeUsernameError(boolean hasError) {
        mUsernameInputLayout.setError(
                hasError ? getString(R.string.password_entry_edit_duplicate_username_error) : "");
        changeDoneButtonState(hasError);
    }

    void changePasswordError(boolean hasError) {
        mPasswordInputLayout.setError(
                hasError ? getString(R.string.password_entry_edit_empty_password_error) : "");
        changeDoneButtonState(hasError);
    }

    void setPassword(String password) {
        // Don't update the text field if it has the same contents, as this will reset the cursor
        // position to the beginning.
        if (mPasswordField.getText().toString().equals(password)) return;
        mPasswordField.setText(password);
    }

    void changePasswordVisibility(boolean visible) {
        if (visible) {
            getActivity().getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
            mPasswordField.setInputType(InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                    | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        } else {
            getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
            mPasswordField.setInputType(InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        }
        ChromeImageButton passwordVisibilityButton =
                getView().findViewById(R.id.password_visibility_button);
        passwordVisibilityButton.setImageResource(
                visible ? R.drawable.ic_visibility_off_black : R.drawable.ic_visibility_black);
    }

    void changeDoneButtonState(boolean hasError) {
        mDoneButton.setEnabled(!hasError);
        mDoneButton.setClickable(!hasError);
    }

    private static void addLayoutChangeListener(TextInputEditText textField, View icons) {
        icons.addOnLayoutChangeListener((View v, int left, int top, int right, int bottom,
                                                int oldLeft, int oldTop, int oldRight,
                                                int oldBottom) -> {
            // Padding at the end of the text to ensure space for the icons.
            ViewCompat.setPaddingRelative(textField, ViewCompat.getPaddingStart(textField),
                    textField.getPaddingTop(), icons.getWidth(), textField.getPaddingBottom());
        });
    }
}
