// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * This class is responsible for rendering a fragment containing details about a saved federated
 * credential.
 */
public class FederatedCredentialFragmentView extends CredentialEntryFragmentViewBase {
    private ChromeImageButton mCopyButton;
    private TextView mUsernameTextView;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        mPageTitle.set(getString(R.string.password_entry_viewer_title));
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
        return inflater.inflate(R.layout.federated_credential_view, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mUsernameTextView = getView().findViewById(R.id.username);
        mCopyButton = getView().findViewById(R.id.copy_username_button);

        View usernameLayout = getView().findViewById(R.id.username_layout);
        TextView usernameLabel = getView().findViewById(R.id.username_label);

        usernameLayout.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> {
                    MarginLayoutParams layoutParams =
                            (MarginLayoutParams) usernameLayout.getLayoutParams();
                    int totalMargin =
                            getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.federated_view_username_margin_bottom);
                    if (mCopyButton.getHeight() < usernameLayout.getHeight()) {
                        layoutParams.bottomMargin = totalMargin;
                    } else {
                        layoutParams.bottomMargin =
                                totalMargin
                                        - (mCopyButton.getHeight()
                                                - mUsernameTextView.getHeight()
                                                - usernameLabel.getHeight());
                    }
                    usernameLayout.setLayoutParams(layoutParams);
                });
    }

    @Override
    void setUiActionHandler(UiActionHandler uiActionHandler) {
        super.setUiActionHandler(uiActionHandler);
        ChromeImageButton mCopyButton = getView().findViewById(R.id.copy_username_button);
        mCopyButton.setOnClickListener(
                (unusedView) ->
                        uiActionHandler.onCopyUsername(getActivity().getApplicationContext()));
    }

    void setUrlOrApp(String urlOrApp) {
        TextView urlOrAppText = getView().findViewById(R.id.url_or_app);
        urlOrAppText.setText(urlOrApp);
    }

    void setUsername(String username) {
        mUsernameTextView.setText(username);
    }

    void setIdentityProvider(String federatedOrigin) {
        TextView passwordText = getView().findViewById(R.id.password);
        passwordText.setText(getString(R.string.password_via_federation, federatedOrigin));
    }
}
