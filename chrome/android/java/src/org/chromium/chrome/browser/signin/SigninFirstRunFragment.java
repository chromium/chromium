// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunFragment;
import org.chromium.chrome.browser.signin.ui.SigninUtils;
import org.chromium.chrome.browser.signin.ui.frebottomgroup.FREBottomGroupCoordinator;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * This fragment handles the sign-in without sync consent during the FRE.
 */
public class SigninFirstRunFragment
        extends Fragment implements FirstRunFragment, FREBottomGroupCoordinator.Listener {
    private static final String FOOTER_LINK_OPEN = "<LINK>";
    private static final String FOOTER_LINK_CLOSE = "</LINK>";

    @VisibleForTesting
    static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    private ModalDialogManager mModalDialogManager;
    // TODO(crbug/1186595): Rename the class FREBottomGroupCoordinator to FreBottomGroupCoordinator
    private @Nullable FREBottomGroupCoordinator mFREBottomGroupCoordinator;
    private boolean mNativeInitialized;

    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        getPageDelegate().getPolicyLoadListener().onAvailable(
                hasPolicies -> notifyCoordinatorWhenNativeAndPolicyAreLoaded());
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.signin_first_run_view, container, false);
        mFREBottomGroupCoordinator =
                new FREBottomGroupCoordinator(requireContext(), view, mModalDialogManager, this);
        final NoUnderlineClickableSpan footerLinkSpan =
                new NoUnderlineClickableSpan(getResources(), this::onFooterLinkClicked);
        SpannableString footerString = SpanApplier.applySpans(getString(R.string.signin_fre_footer),
                new SpanApplier.SpanInfo(FOOTER_LINK_OPEN, FOOTER_LINK_CLOSE, footerLinkSpan));
        TextViewWithClickableSpans footerView = view.findViewById(R.id.signin_fre_footer);
        footerView.setText(footerString);
        footerView.setMovementMethod(LinkMovementMethod.getInstance());
        notifyCoordinatorWhenNativeAndPolicyAreLoaded();
        return view;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mFREBottomGroupCoordinator.destroy();
    }

    /**
     * Implements {@link FirstRunFragment}.
     */
    @Override
    public void setInitialA11yFocus() {}

    /**
     * Implements {@link FirstRunFragment}.
     */
    @Override
    public void onNativeInitialized() {
        mNativeInitialized = true;
        notifyCoordinatorWhenNativeAndPolicyAreLoaded();
    }

    /**
     * Implements {@link FREBottomGroupCoordinator.Listener}.
     */
    @Override
    public void addAccount() {
        AccountManagerFacadeProvider.getInstance().createAddAccountIntent(
                (@Nullable Intent intent) -> {
                    if (intent != null) {
                        startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                        return;
                    }

                    // AccountManagerFacade couldn't create intent, use SigninUtils to open settings
                    // instead.
                    SigninUtils.openSettingsForAllAccounts(getActivity());
                });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE && resultCode == Activity.RESULT_OK
                && data != null) {
            String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (addedAccountName != null) {
                mFREBottomGroupCoordinator.onAccountSelected(addedAccountName);
            }
        }
    }

    /**
     * Implements {@link FREBottomGroupCoordinator.Listener}.
     */
    @Override
    public void advanceToNextPage() {
        getPageDelegate().acceptTermsOfService(true);
    }

    private void notifyCoordinatorWhenNativeAndPolicyAreLoaded() {
        if (mFREBottomGroupCoordinator != null && mNativeInitialized
                && getPageDelegate().getPolicyLoadListener().get() != null) {
            mFREBottomGroupCoordinator.onNativeAndPolicyLoaded(
                    getPageDelegate().getPolicyLoadListener().get());
        }
    }

    private void onFooterLinkClicked(View view) {}
}