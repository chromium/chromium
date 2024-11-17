// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.content.Context;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

/**
 * Base structure to be shared by fragments displaying: saved credentials to be edited, saved
 * federated credentials and sites blocklisted for saving by the user.
 */
public abstract class CredentialEntryFragmentViewBase extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    ComponentStateDelegate mComponentStateDelegate;
    UiActionHandler mUiActionHandler;

    /**
     * To be implemented by classes which need to know about the fragment's state
     * TODO(crbug.com/40749164): The coordinator should be made a LifecycleObserver instead.
     */
    interface ComponentStateDelegate {
        /** Called when the fragment is started. */
        void onStartFragment();

        /** Called when the fragment is resumed. */
        void onResumeFragment();

        /** Signals that the component is no longer needed. */
        void onDestroy();
    }

    /**
     * Handler for the various actions available in the UI: removing credentials, copying the
     * username, copying the password, etc.
     */
    interface UiActionHandler {
        /** Called when the user clicks the button to mask/unmask the password */
        void onMaskOrUnmaskPassword();

        /** Called when the user clicks the button to delete the credential */
        void onDelete();

        /** Called when the help icon is clicked */
        void handleHelp();

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

    void setComponentStateDelegate(ComponentStateDelegate stateDelegate) {
        mComponentStateDelegate = stateDelegate;
    }

    void setUiActionHandler(UiActionHandler actionHandler) {
        mUiActionHandler = actionHandler;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();

        inflater.inflate(R.menu.credential_edit_action_bar_menu, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (mUiActionHandler == null) return super.onOptionsItemSelected(item);

        int id = item.getItemId();
        if (id == R.id.action_delete_saved_password) {
            mUiActionHandler.onDelete();
            return true;
        }
        if (id == R.id.help_button) {
            mUiActionHandler.handleHelp();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onStart() {
        super.onStart();
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
}
