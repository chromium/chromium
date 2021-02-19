// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * This class is responsible for rendering the edit fragment where users can edit a saved password.
 */
public class CredentialEditFragmentView extends PreferenceFragmentCompat {
    private ComponentStateDelegate mComponentStateDelegate;

    // TODO(crbug.com/1178519): The coordinator should be made a LifecycleObserver instead.
    interface ComponentStateDelegate {
        /**
         * Called when the fragment is started.
         */
        void onStartFragment();

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
        if (mComponentStateDelegate != null) mComponentStateDelegate.onStartFragment();

        EditText usernameField = getView().findViewById(R.id.username);
        View usernameIcon = getView().findViewById(R.id.copy_username_button);
        addLayoutChangeListener(usernameField, usernameIcon);

        EditText passwordField = getView().findViewById(R.id.password);
        View passwordIcons = getView().findViewById(R.id.password_icons);
        addLayoutChangeListener(passwordField, passwordIcons);
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

    void setUrlOrApp(String urlOrApp) {
        TextView urlOrAppText = getView().findViewById(R.id.url_or_app);
        urlOrAppText.setText(urlOrApp);

        TextView editInfoText = getView().findViewById(R.id.edit_info);
        editInfoText.setText(getString(R.string.password_edit_hint, urlOrApp));
    }

    void setUsername(String username) {
        EditText usernameField = getView().findViewById(R.id.username);
        usernameField.setText(username);
    }

    void setPassword(String password) {
        EditText passwordField = getView().findViewById(R.id.password);
        passwordField.setText(password);
    }

    void changePasswordVisibility(boolean visible) {
        EditText passwordText = getView().findViewById(R.id.password);

        if (visible) {
            getActivity().getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
            passwordText.setInputType(InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                    | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        } else {
            getActivity().getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
            passwordText.setInputType(InputType.TYPE_CLASS_TEXT
                    | InputType.TYPE_TEXT_VARIATION_PASSWORD | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        }
        ChromeImageButton viewPasswordButton = getView().findViewById(R.id.view_password_button);
        viewPasswordButton.setImageResource(
                visible ? R.drawable.ic_visibility_off_black : R.drawable.ic_visibility_black);
    }

    private void addLayoutChangeListener(EditText textField, View icons) {
        icons.addOnLayoutChangeListener((View v, int left, int top, int right, int bottom,
                                                int oldLeft, int oldTop, int oldRight,
                                                int oldBottom) -> {
            // Padding at the end of the text to ensure space for the icons.
            ViewCompat.setPaddingRelative(textField, ViewCompat.getPaddingStart(textField),
                    textField.getPaddingTop(), icons.getWidth(), textField.getPaddingBottom());
        });
    }
}
