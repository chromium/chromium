// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * This class is responsible for rendering the edit fragment where users can edit a saved password.
 */
public class CredentialEditFragmentView extends CredentialEntryFragmentViewBase {
    private TextInputLayout mUsernameInputLayout;
    private TextInputEditText mUsernameField;
    private TextInputLayout mPasswordInputLayout;
    private TextInputEditText mPasswordField;
    private ButtonCompat mDoneButton;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        mPageTitle.set(getString(R.string.password_entry_viewer_edit_stored_password_action_title));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        return inflater.inflate(R.layout.credential_edit_view, container, false);
    }

    @Override
    public void onStart() {
        mUsernameInputLayout = getView().findViewById(R.id.username_text_input_layout);
        mUsernameField = getView().findViewById(R.id.username);
        View usernameIcon = getView().findViewById(R.id.copy_username_button);
        addLayoutChangeListener(mUsernameField, usernameIcon);

        mPasswordInputLayout = getView().findViewById(R.id.password_text_input_layout);
        mPasswordField = getView().findViewById(R.id.password);
        View passwordIcons = getView().findViewById(R.id.password_icons);
        addLayoutChangeListener(mPasswordField, passwordIcons);

        mDoneButton = getView().findViewById(R.id.button_primary);

        getView().findViewById(R.id.button_secondary).setOnClickListener((unusedView) -> dismiss());

        super.onStart();
    }

    @Override
    void setUiActionHandler(UiActionHandler uiActionHandler) {
        super.setUiActionHandler(uiActionHandler);

        ChromeImageButton usernameCopyButton = getView().findViewById(R.id.copy_username_button);
        usernameCopyButton.setOnClickListener(
                (unusedView) ->
                        uiActionHandler.onCopyUsername(getActivity().getApplicationContext()));

        ChromeImageButton passwordCopyButton = getView().findViewById(R.id.copy_password_button);
        passwordCopyButton.setOnClickListener(
                (unusedView) ->
                        uiActionHandler.onCopyPassword(getActivity().getApplicationContext()));

        ChromeImageButton passwordVisibilityButton =
                getView().findViewById(R.id.password_visibility_button);
        passwordVisibilityButton.setOnClickListener(
                (unusedView) -> uiActionHandler.onMaskOrUnmaskPassword());

        getView()
                .findViewById(R.id.button_primary)
                .setOnClickListener(
                        (unusedView) -> {
                            uiActionHandler.onSave();
                            dismiss();
                        });

        getView().findViewById(R.id.button_secondary).setOnClickListener((unusedView) -> dismiss());

        mUsernameField.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        uiActionHandler.onUsernameTextChanged(charSequence.toString());
                    }
                });

        mPasswordField.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
                        uiActionHandler.onPasswordTextChanged(charSequence.toString());
                    }
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
            getActivity()
                    .getWindow()
                    .setFlags(
                            WindowManager.LayoutParams.FLAG_SECURE,
                            WindowManager.LayoutParams.FLAG_SECURE);
            mPasswordField.setInputType(
                    InputType.TYPE_CLASS_TEXT
                            | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                            | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        } else {
            getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
            mPasswordField.setInputType(
                    InputType.TYPE_CLASS_TEXT
                            | InputType.TYPE_TEXT_VARIATION_PASSWORD
                            | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        }
        ChromeImageButton passwordVisibilityButton =
                getView().findViewById(R.id.password_visibility_button);
        passwordVisibilityButton.setImageResource(
                visible ? R.drawable.ic_visibility_off_black : R.drawable.ic_visibility_black);
        passwordVisibilityButton.setContentDescription(
                visible
                        ? getString(R.string.password_entry_viewer_hide_stored_password)
                        : getString(R.string.password_entry_viewer_show_stored_password));
    }

    void changeDoneButtonState(boolean hasError) {
        mDoneButton.setEnabled(!hasError);
        mDoneButton.setClickable(!hasError);
    }

    private static void addLayoutChangeListener(TextInputEditText textField, View icons) {
        icons.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> {
                    // Padding at the end of the text to ensure space for the icons.
                    textField.setPaddingRelative(
                            ViewCompat.getPaddingStart(textField),
                            textField.getPaddingTop(),
                            icons.getWidth(),
                            textField.getPaddingBottom());
                });
    }
}
