// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
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

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.firstrun.FirstRunFragment;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.firstrun.SkipTosDialogPolicyListener;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.device_lock.DeviceLockCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninView;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/** This fragment handles the sign-in without sync consent during the FRE. */
public class SigninFirstRunFragment extends Fragment
        implements FirstRunFragment,
                FullscreenSigninCoordinator.Delegate,
                DeviceLockCoordinator.Delegate {
    @VisibleForTesting static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    // Used as a view holder for the current orientation of the device.
    private FrameLayout mFragmentView;
    private View mMainView;
    private ModalDialogManager mModalDialogManager;
    private SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;
    private FullscreenSigninCoordinator mFullscreenSigninCoordinator;
    private DeviceLockCoordinator mDeviceLockCoordinator;
    private boolean mExitFirstRunCalled;
    private boolean mDelayedExitFirstRunCalledForTesting;

    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
        mFullscreenSigninCoordinator =
                new FullscreenSigninCoordinator(
                        requireContext(),
                        mModalDialogManager,
                        this,
                        PrivacyPreferencesManagerImpl.getInstance(),
                        SigninAccessPoint.START_PAGE);

        if (getPageDelegate().isLaunchedFromCct()) {
            mSkipTosDialogPolicyListener =
                    new SkipTosDialogPolicyListener(
                            getPageDelegate().getPolicyLoadListener(),
                            EnterpriseInfo.getInstance(),
                            null);
            mSkipTosDialogPolicyListener.onAvailable(
                    (Boolean skipTos) -> {
                        if (skipTos) exitFirstRun();
                    });
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mFragmentView = null;
        if (mSkipTosDialogPolicyListener != null) {
            mSkipTosDialogPolicyListener.destroy();
            mSkipTosDialogPolicyListener = null;
        }
        mFullscreenSigninCoordinator.destroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Keep device lock page if it's currently displayed.
        if (mDeviceLockCoordinator != null) {
            return;
        }
        // Inflate the view required for the current configuration and set it as the fragment view.
        mFragmentView.removeAllViews();
        mMainView =
                inflateFragmentView(
                        (LayoutInflater)
                                getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE),
                        getActivity());
        mFragmentView.addView(mMainView);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mFragmentView = new FrameLayout(getActivity());
        mMainView = inflateFragmentView(inflater, getActivity());
        mFragmentView.addView(mMainView);

        return mFragmentView;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == ADD_ACCOUNT_REQUEST_CODE
                && resultCode == Activity.RESULT_OK
                && data != null) {
            String addedAccountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
            if (addedAccountName != null) {
                mFullscreenSigninCoordinator.onAccountSelected(addedAccountName);
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
    public void reset() {
        mFullscreenSigninCoordinator.reset();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void addAccount() {
        recordFreProgressHistogram(MobileFreProgress.WELCOME_ADD_ACCOUNT);
        AccountManagerFacadeProvider.getInstance()
                .createAddAccountIntent(
                        (@Nullable Intent intent) -> {
                            if (intent != null) {
                                startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                                return;
                            }

                            // AccountManagerFacade couldn't create intent, use SigninUtils to open
                            // settings instead.
                            SigninUtils.openSettingsForAllAccounts(getActivity());
                        });
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {
        getPageDelegate().acceptTermsOfService(allowMetricsAndCrashUploading);
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void advanceToNextPage() {
        getPageDelegate().advanceToNextPage();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void recordFreProgressHistogram(@MobileFreProgress int state) {
        getPageDelegate().recordFreProgressHistogram(state);
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void recordNativePolicyAndChildStatusLoadedHistogram() {
        getPageDelegate().recordNativePolicyAndChildStatusLoadedHistogram();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void recordNativeInitializedHistogram() {
        getPageDelegate().recordNativeInitializedHistogram();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void showInfoPage(@StringRes int url) {
        getPageDelegate().showInfoPage(url);
    }

    @Override
    public OneshotSupplier<ProfileProvider> getProfileSupplier() {
        return getPageDelegate().getProfileProviderSupplier();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public OneshotSupplier<Boolean> getPolicyLoadListener() {
        return getPageDelegate().getPolicyLoadListener();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public OneshotSupplier<Boolean> getChildAccountStatusSupplier() {
        return getPageDelegate().getChildAccountStatusSupplier();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return getPageDelegate().getNativeInitializationPromise();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public boolean shouldDisplayManagementNoticeOnManagedDevices() {
        return true;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public boolean shouldDisplayFooterText() {
        return true;
    }

    @MainThread
    private void exitFirstRun() {
        // Make sure this function is called at most once.
        if (!mExitFirstRunCalled) {
            mExitFirstRunCalled = true;
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        mDelayedExitFirstRunCalledForTesting = true;

                        // If we've been detached, someone else has handled something, and it's no
                        // longer clear that we should still be accepting the ToS and exiting the
                        // FRE.
                        if (isDetached()) return;

                        getPageDelegate().acceptTermsOfService(false);
                        getPageDelegate().exitFirstRun();
                    },
                    FirstRunUtils.getSkipTosExitDelayMs());
        }
    }

    private View inflateFragmentView(LayoutInflater inflater, Activity activity) {
        boolean useLandscapeLayout = SigninUtils.shouldShowDualPanesHorizontalLayout(activity);

        final FullscreenSigninView view =
                (FullscreenSigninView)
                        inflater.inflate(
                                useLandscapeLayout
                                        ? R.layout.fullscreen_signin_landscape_view
                                        : R.layout.fullscreen_signin_portrait_view,
                                null,
                                false);
        mFullscreenSigninCoordinator.setView(view);
        return view;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public void displayDeviceLockPage(Account selectedAccount) {
        Profile profile = ProfileProvider.getOrCreateProfile(getProfileSupplier().get(), false);
        mDeviceLockCoordinator =
                new DeviceLockCoordinator(
                        this,
                        getPageDelegate().getWindowAndroid(),
                        profile,
                        getActivity(),
                        selectedAccount);
    }

    /** Implements {@link DeviceLockCoordinator.Delegate}. */
    @Override
    public void setView(View view) {
        mFragmentView.removeAllViews();
        mFragmentView.addView(view);
    }

    /** Implements {@link DeviceLockCoordinator.Delegate}. */
    @Override
    public void onDeviceLockReady() {
        if (mFragmentView != null) {
            restoreMainView();
        }
        if (mDeviceLockCoordinator != null) {
            mDeviceLockCoordinator.destroy();
            mDeviceLockCoordinator = null;

            // Hold off on continuing sign-in if the delegate is null (due to the host activity
            // being killed in the background.
            if (getPageDelegate() != null) {
                mFullscreenSigninCoordinator.continueSignIn();
            }
        }
    }

    /** Implements {@link DeviceLockCoordinator.Delegate}. */
    @Override
    public void onDeviceLockRefused() {
        mFullscreenSigninCoordinator.cancelSignInAndDismiss();
    }

    @Override
    public @DeviceLockActivityLauncher.Source String getSource() {
        return DeviceLockActivityLauncher.Source.FIRST_RUN;
    }

    private void restoreMainView() {
        mFragmentView.removeAllViews();
        mFragmentView.addView(mMainView);
    }

    boolean getDelayedExitFirstRunCalledForTesting() {
        return mDelayedExitFirstRunCalledForTesting;
    }
}
