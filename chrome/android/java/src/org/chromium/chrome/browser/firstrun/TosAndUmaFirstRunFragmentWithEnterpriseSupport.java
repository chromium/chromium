// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.components.browser_ui.widget.LoadingView;
import org.chromium.policy.PolicyService;

/**
 * Another FirstRunFragment that is only used when running with CCT.
 */
public class TosAndUmaFirstRunFragmentWithEnterpriseSupport
        extends ToSAndUMAFirstRunFragment implements LoadingView.Observer {
    private static final String TAG = "TosAndUmaFragment";

    /** FRE page that instantiates this fragment. */
    public static class Page
            implements FirstRunPage<TosAndUmaFirstRunFragmentWithEnterpriseSupport> {
        @Override
        public boolean shouldSkipPageOnCreate() {
            // TODO(crbug.com/1111490): Revisit during post-MVP.
            // There's an edge case where we accept the welcome page in the main app, abort the FRE,
            // then go through this CCT FRE again.
            return FirstRunStatus.shouldSkipWelcomePage();
        }

        @Override
        public TosAndUmaFirstRunFragmentWithEnterpriseSupport instantiateFragment() {
            return new TosAndUmaFirstRunFragmentWithEnterpriseSupport();
        }
    }

    private boolean mViewCreated;
    private View mLoadingSpinnerContainer;
    private LoadingView mLoadingSpinner;
    private CallbackController mCallbackController;
    private PolicyService.Observer mPolicyServiceObserver;

    /** The {@link SystemClock} timestamp when this object was created. */
    private long mObjectCreatedTimeMs;

    /** The {@link SystemClock} timestamp when onViewCreated is called. */
    private long mViewCreatedTimeMs;

    /**
     * Whether app restriction is found on the device. This can be null when this information is not
     * ready yet.
     */
    private @Nullable Boolean mHasRestriction;
    /**
     * The value of CCTToSDialogEnabled policy on the device. If the value is false, it means we
     * should skip the rest of FRE. This can be null when this information is not ready yet.
     */
    private @Nullable Boolean mPolicyCctTosDialogEnabled;
    /**
     * Whether the current device is organization owned. This will start null before the check
     * occurs. The FRE can only be skipped if the device is owned corporate owned.
     */
    private @Nullable Boolean mIsDeviceOwned;

    private TosAndUmaFirstRunFragmentWithEnterpriseSupport() {
        mObjectCreatedTimeMs = SystemClock.elapsedRealtime();
        mCallbackController = new CallbackController();
    }

    @Override
    public void onDestroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mLoadingSpinner != null) {
            mLoadingSpinner.destroy();
            mLoadingSpinner = null;
        }
        if (mPolicyServiceObserver != null) {
            PolicyServiceFactory.getGlobalPolicyService().removeObserver(mPolicyServiceObserver);
            mPolicyServiceObserver = null;
        }
        super.onDestroy();
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        checkAppRestriction();
        // It's possible for app restrictions to have its callback synchronously invoked and we can
        // give up on the skip scenario.
        if (shouldWaitForPolicyLoading()) {
            checkIsDeviceOwned();
        }
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mLoadingSpinnerContainer = view.findViewById(R.id.loading_view_container);
        mLoadingSpinner = view.findViewById(R.id.progress_spinner_large);
        mViewCreated = true;
        mViewCreatedTimeMs = SystemClock.elapsedRealtime();

        if (shouldWaitForPolicyLoading()) {
            mLoadingSpinner.addObserver(this);
            mLoadingSpinner.showLoadingUI();
            setTosAndUmaVisible(false);
        } else if (confirmedCctTosDialogDisabled()) {
            // Skip the FRE if we know dialog is disabled by policy.
            exitCctFirstRun();
        }
    }

    @Override
    public void onNativeInitialized() {
        super.onNativeInitialized();

        if (shouldWaitForPolicyLoading()) {
            checkEnterprisePolicies();
        }
    }

    @Override
    protected boolean canShowUmaCheckBox() {
        return super.canShowUmaCheckBox() && confirmedToShowUmaAndTos();
    }

    @Override
    public void onShowLoadingUIComplete() {
        mLoadingSpinnerContainer.setVisibility(View.VISIBLE);
    }

    @Override
    public void onHideLoadingUIComplete() {
        RecordHistogram.recordTimesHistogram("MobileFre.CctTos.LoadingDuration",
                SystemClock.elapsedRealtime() - mViewCreatedTimeMs);
        if (confirmedCctTosDialogDisabled() && confirmedOwnedDevice()) {
            // TODO(crbug.com/1108564): Show the different UI that has the enterprise disclosure.
            exitCctFirstRun();
        } else {
            // Else, show the UMA as the loading spinner is GONE.
            assert confirmedToShowUmaAndTos();

            boolean hasAccessibilityFocus = mLoadingSpinnerContainer.isAccessibilityFocused();
            mLoadingSpinnerContainer.setVisibility(View.GONE);
            setTosAndUmaVisible(true);

            if (hasAccessibilityFocus) {
                getToSAndPrivacyText().sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        }
    }

    /**
     * @return True if we need to wait on an Async tasks that determine whether any Enterprise
     *         policies needs to be applied. If this returns false, then we no longer need to wait
     *         and can update the UI immediately.
     */
    private boolean shouldWaitForPolicyLoading() {
        // Note that someSignalOutstanding doesn't care about mHasRestriction. It's main purpose is
        // to be a very quick signal mPolicyCctTosDialogEnabled is never going to turn false. But
        // once mPolicyCctTosDialogEnabled has a non-null value, mHasRestriction is redundant. It
        // never actually needs to return for us to know we can skip the ToS.
        boolean someSignalOutstanding =
                mPolicyCctTosDialogEnabled == null || mIsDeviceOwned == null;
        boolean mightStillBeAllowedToSkip = !confirmedToShowUmaAndTos();
        return someSignalOutstanding && mightStillBeAllowedToSkip;
    }

    /**
     * This methods will return true only when we know either 1) there's no on-device app
     * restrictions or 2) policies has been loaded and first run has not been disabled via policy.
     *
     * @return Whether we should show TosAndUma components on the UI.
     */
    private boolean confirmedToShowUmaAndTos() {
        return confirmedNoAppRestriction() || confirmedCctTosDialogEnabled()
                || confirmedNotOwnedDevice();
    }

    private boolean confirmedNoAppRestriction() {
        return mHasRestriction != null && !mHasRestriction;
    }

    private boolean confirmedCctTosDialogEnabled() {
        return mPolicyCctTosDialogEnabled != null && mPolicyCctTosDialogEnabled;
    }

    private boolean confirmedCctTosDialogDisabled() {
        return mPolicyCctTosDialogEnabled != null && !mPolicyCctTosDialogEnabled;
    }

    private boolean confirmedNotOwnedDevice() {
        return mIsDeviceOwned != null && !mIsDeviceOwned;
    }

    private boolean confirmedOwnedDevice() {
        return mIsDeviceOwned != null && mIsDeviceOwned;
    }

    private void checkAppRestriction() {
        getPageDelegate().getFirstRunAppRestrictionInfo().getHasAppRestriction(
                mCallbackController.makeCancelable(this::onAppRestrictionDetected));
    }

    private void onAppRestrictionDetected(boolean hasAppRestriction) {
        // It's possible that we've already told the spinner to hide, and even signaled to our
        // delegate to exit. If so, we can ignore the app restrictions value.
        // TODO(https://crbug.com/1119449): Shouldn't need this check if we can cancel this callback
        // when we no longer need it.
        if (!shouldWaitForPolicyLoading()) {
            return;
        }

        mHasRestriction = hasAppRestriction;
        maybeHideSpinner();
    }

    private void checkEnterprisePolicies() {
        PolicyService policyService = PolicyServiceFactory.getGlobalPolicyService();
        if (policyService.isInitializationComplete()) {
            updateCctTosPolicy();
        } else {
            mPolicyServiceObserver = () -> {
                policyService.removeObserver(mPolicyServiceObserver);
                mPolicyServiceObserver = null;
                updateCctTosPolicy();
            };
            policyService.addObserver(mPolicyServiceObserver);
        }
    }

    private void updateCctTosPolicy() {
        mPolicyCctTosDialogEnabled = FirstRunUtils.isCctTosDialogEnabled();
        maybeHideSpinner();

        RecordHistogram.recordTimesHistogram(mViewCreated
                        ? "MobileFre.CctTos.EnterprisePolicyCheckSpeed.SlowerThanInflation"
                        : "MobileFre.CctTos.EnterprisePolicyCheckSpeed.FasterThanInflation",
                SystemClock.elapsedRealtime() - mObjectCreatedTimeMs);
    }

    private void checkIsDeviceOwned() {
        EnterpriseInfo.getInstance().getDeviceEnterpriseInfo(
                mCallbackController.makeCancelable(this::onIsDeviceOwnedDetected));
    }

    private void onIsDeviceOwnedDetected(EnterpriseInfo.OwnedState ownedState) {
        // If unable to determine the owned state then fail closed, no skipping.
        mIsDeviceOwned = ownedState != null && ownedState.mDeviceOwned;
        maybeHideSpinner();

        RecordHistogram.recordTimesHistogram(mViewCreated
                        ? "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.SlowerThanInflation"
                        : "MobileFre.CctTos.IsDeviceOwnedCheckSpeed.FasterThanInflation",
                SystemClock.elapsedRealtime() - mObjectCreatedTimeMs);
    }

    private void maybeHideSpinner() {
        if (!shouldWaitForPolicyLoading() && mViewCreated) {
            // TODO(https://crbug.com/1119449): Cleanup various policy callbacks.
            mLoadingSpinner.hideLoadingUI();
        }
    }

    private void exitCctFirstRun() {
        assert confirmedCctTosDialogDisabled();
        assert confirmedOwnedDevice();
        // TODO(crbug.com/1108564): Fire a signal to end this fragment when disclaimer is ready.
        // TODO(crbug.com/1108582): Save a shared pref indicating Enterprise CCT FRE is complete,
        //  and skip waiting for future cold starts.
        Log.d(TAG, "TosAndUmaFirstRunFragmentWithEnterpriseSupport finished.");
        getPageDelegate().exitFirstRun();
    }
}
