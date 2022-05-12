// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.firstrun.FirstRunFragment;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.firstrun.SkipTosDialogPolicyListener;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.fre.FreUMADialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunCoordinator;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunView;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * This fragment handles the sign-in without sync consent during the FRE.
 */
public class SigninFirstRunFragment extends Fragment implements FirstRunFragment,
                                                                SigninFirstRunCoordinator.Delegate,
                                                                FreUMADialogCoordinator.Listener {
    @VisibleForTesting
    static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    // Used as a view holder for the current orientation of the device.
    private FrameLayout mFragmentView;
    private ModalDialogManager mModalDialogManager;
    private SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;
    private @Nullable SigninFirstRunCoordinator mSigninFirstRunCoordinator;
    private boolean mExitFirstRunCalled;
    private boolean mNativeInitialized;
    private boolean mNativePolicyAndChildStatusLoaded;
    private boolean mAllowCrashUpload;

    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        getPageDelegate().getPolicyLoadListener().onAvailable(
                hasPolicies -> notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded());
        getPageDelegate().getChildAccountStatusSupplier().onAvailable(
                ignored -> notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded());
        if (getPageDelegate().isLaunchedFromCct()) {
            mSkipTosDialogPolicyListener = new SkipTosDialogPolicyListener(
                    getPageDelegate().getPolicyLoadListener(), EnterpriseInfo.getInstance(), null);
            mSkipTosDialogPolicyListener.onAvailable((Boolean skipTos) -> {
                if (skipTos) exitFirstRun();
            });
        }
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mFragmentView = null;
        if (mSkipTosDialogPolicyListener != null) {
            mSkipTosDialogPolicyListener.destroy();
            mSkipTosDialogPolicyListener = null;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Inflate the view required for the current configuration and set it as the fragment view.
        mFragmentView.removeAllViews();
        mFragmentView.addView(inflateFragmentView(
                (LayoutInflater) getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE),
                newConfig));
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mAllowCrashUpload = false;
        mFragmentView = new FrameLayout(getActivity());
        mFragmentView.addView(inflateFragmentView(inflater, getResources().getConfiguration()));

        return mFragmentView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        setSigninFirstRunCoordinator(null);
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
        notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded();
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void reset() {
        if (mSigninFirstRunCoordinator != null) {
            mSigninFirstRunCoordinator.reset();
        }
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
    public void acceptTermsOfService() {
        getPageDelegate().acceptTermsOfService(mAllowCrashUpload);
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void advanceToNextPage() {
        getPageDelegate().advanceToNextPage();
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void recordFreProgressHistogram(@MobileFreProgress int state) {
        getPageDelegate().recordFreProgressHistogram(state);
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void showInfoPage(@StringRes int url) {
        getPageDelegate().showInfoPage(url);
    }

    /** Implements {@link SigninFirstRunCoordinator.Delegate}. */
    @Override
    public void openUmaDialog() {
        new FreUMADialogCoordinator(requireContext(), mModalDialogManager, this, mAllowCrashUpload);
    }

    /** Implements {@link FreUMADialogCoordinator.Listener} */
    @Override
    public void onAllowCrashUploadChecked(boolean allowCrashUpload) {
        mAllowCrashUpload = allowCrashUpload;
    }

    @MainThread
    private void exitFirstRun() {
        // Make sure this function is called at most once.
        if (!mExitFirstRunCalled) {
            mExitFirstRunCalled = true;
            new Handler().postDelayed(() -> {
                getPageDelegate().acceptTermsOfService(false);
                getPageDelegate().exitFirstRun();
            }, FirstRunUtils.getSkipTosExitDelayMs());
        }
    }

    /**
     * Destroys the old coordinator if needed and sets {@link #mSigninFirstRunCoordinator}.
     * @param coordinator the new coordinator instance (may be null).
     */
    private void setSigninFirstRunCoordinator(@Nullable SigninFirstRunCoordinator coordinator) {
        if (mSigninFirstRunCoordinator != null) {
            mSigninFirstRunCoordinator.destroy();
        }
        mSigninFirstRunCoordinator = coordinator;
        if (mSigninFirstRunCoordinator != null && mNativePolicyAndChildStatusLoaded) {
            mSigninFirstRunCoordinator.onNativePolicyAndChildStatusLoaded(
                    getPageDelegate().getPolicyLoadListener().get());
        }
    }

    /**
     * Notifies the coordinator that native, policies and child account status has been loaded.
     * This method may be called multiple times after all 3 wait conditions have been satisfied.
     */
    private void notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded() {
        // This may happen when the native initialized supplier in FirstRunActivity calls back after
        // the fragment has been detached from the activity. See https://crbug.com/1294998.
        if (getPageDelegate() == null) return;

        if (mSigninFirstRunCoordinator != null && mNativeInitialized
                && getPageDelegate().getChildAccountStatusSupplier().get() != null
                && getPageDelegate().getPolicyLoadListener().get() != null) {
            // Only notify once.
            if (!mNativePolicyAndChildStatusLoaded) {
                mNativePolicyAndChildStatusLoaded = true;
                mAllowCrashUpload =
                        !mSigninFirstRunCoordinator.isMetricsReportingDisabledByPolicy();
                mSigninFirstRunCoordinator.onNativePolicyAndChildStatusLoaded(
                        getPageDelegate().getPolicyLoadListener().get());
                getPageDelegate().recordNativePolicyAndChildStatusLoadedHistogram();
            }
        }
    }

    private View inflateFragmentView(LayoutInflater inflater, Configuration configuration) {
        // Since the landscape view has two panes the minimum screenWidth to show it is set to
        // 600dp per android guideline.
        final SigninFirstRunView view = (SigninFirstRunView) inflater.inflate(
                configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                                && configuration.screenWidthDp >= 600
                        ? R.layout.signin_first_run_landscape_view
                        : R.layout.signin_first_run_portrait_view,
                null, false);
        setSigninFirstRunCoordinator(new SigninFirstRunCoordinator(requireContext(), view,
                mModalDialogManager, this, PrivacyPreferencesManagerImpl.getInstance()));
        notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded();
        return view;
    }
}
