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
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFragment;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.fre.FreUMADialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunCoordinator;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * This fragment handles the sign-in without sync consent during the FRE.
 */
public class SigninFirstRunFragment extends Fragment implements FirstRunFragment,
                                                                SigninFirstRunCoordinator.Delegate,
                                                                FreUMADialogCoordinator.Listener {
    @VisibleForTesting
    static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    private ModalDialogManager mModalDialogManager;
    private @Nullable SigninFirstRunCoordinator mSigninFirstRunCoordinator;
    private boolean mNativeInitialized;
    private boolean mAllowCrashUpload;

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
        mSigninFirstRunCoordinator =
                new SigninFirstRunCoordinator(requireContext(), view, mModalDialogManager, this);
        notifyCoordinatorWhenNativeAndPolicyAreLoaded();

        mAllowCrashUpload = true;
        setUpFooter(view.findViewById(R.id.signin_fre_footer));
        return view;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mSigninFirstRunCoordinator.destroy();
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        final View title = getView().findViewById(R.id.title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void onNativeInitialized() {
        mNativeInitialized = true;
        notifyCoordinatorWhenNativeAndPolicyAreLoaded();
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void addAccount() {
        recordFreProgressHistogram(MobileFreProgress.WELCOME_ADD_ACCOUNT);
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

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void recordFreProgressHistogram(@MobileFreProgress int state) {
        getPageDelegate().recordFreProgressHistogram(state);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE && resultCode == Activity.RESULT_OK
                && data != null) {
            String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (addedAccountName != null) {
                mSigninFirstRunCoordinator.onAccountSelected(addedAccountName);
            }
        }
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void acceptTermsOfService() {
        getPageDelegate().acceptTermsOfService(mAllowCrashUpload);
    }

    /** Implements {@link FreUMADialogCoordinator.Listener} */
    @Override
    public void onAllowCrashUploadChecked(boolean allowCrashUpload) {
        mAllowCrashUpload = allowCrashUpload;
    }

    private void notifyCoordinatorWhenNativeAndPolicyAreLoaded() {
        if (mSigninFirstRunCoordinator != null && mNativeInitialized
                && getPageDelegate().getPolicyLoadListener().get() != null) {
            mSigninFirstRunCoordinator.onNativeAndPolicyLoaded(
                    getPageDelegate().getPolicyLoadListener().get());
        }
    }

    private void setUpFooter(TextViewWithClickableSpans footerView) {
        final Callback<View> onTermsOfServiceSpanClickListener = view -> {
            CustomTabActivity.showInfoPage(requireContext(),
                    LocalizationUtils.substituteLocalePlaceholder(
                            getString(R.string.google_terms_of_service_url)));
        };
        final Callback<View> onUmaDialogSpanClickListener = view -> {
            new FreUMADialogCoordinator(
                    requireContext(), mModalDialogManager, this, mAllowCrashUpload);
        };
        final NoUnderlineClickableSpan clickableTermsOfServiceSpan =
                new NoUnderlineClickableSpan(getResources(), onTermsOfServiceSpanClickListener);
        final NoUnderlineClickableSpan clickableUMADialogSpan =
                new NoUnderlineClickableSpan(getResources(), onUmaDialogSpanClickListener);
        final SpannableString footerString = SpanApplier.applySpans(
                getString(R.string.signin_fre_footer),
                new SpanApplier.SpanInfo("<LINK1>", "</LINK1>", clickableTermsOfServiceSpan),
                new SpanApplier.SpanInfo("<LINK2>", "</LINK2>", clickableUMADialogSpan));
        footerView.setText(footerString);
        footerView.setMovementMethod(LinkMovementMethod.getInstance());
    }
}
