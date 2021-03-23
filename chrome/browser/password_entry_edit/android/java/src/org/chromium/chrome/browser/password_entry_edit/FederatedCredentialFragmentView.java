// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.ui.widget.ChromeImageButton;

/**
 * This class is responsible for rendering a fragment containing details about a saved federated
 * credential.
 */
public class FederatedCredentialFragmentView extends CredentialEntryFragmentViewBase {
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.password_entry_viewer_title);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        setHasOptionsMenu(true);
        return inflater.inflate(R.layout.federated_credential_view, container, false);
    }

    @Override
    void setUiActionHandler(UiActionHandler uiActionHandler) {
        super.setUiActionHandler(uiActionHandler);
        ChromeImageButton usernameCopyButton = getView().findViewById(R.id.copy_username_button);
        usernameCopyButton.setOnClickListener(
                (unusedView)
                        -> uiActionHandler.onCopyUsername(getActivity().getApplicationContext()));
    }

    void setUrlOrApp(String urlOrApp) {
        TextView urlOrAppText = getView().findViewById(R.id.url_or_app);
        urlOrAppText.setText(urlOrApp);
    }

    void setUsername(String username) {
        TextView usernameText = getView().findViewById(R.id.username);
        usernameText.setText(username);
    }

    void setIdentityProvider(String federatedOrigin) {
        TextView passwordText = getView().findViewById(R.id.password);
        passwordText.setText(getString(R.string.password_via_federation, federatedOrigin));
    }
}
