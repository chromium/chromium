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

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.components.browser_ui.widget.LoadingView;
import org.chromium.components.policy.PolicyService;

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

    private class CctTosFragmentMetricsNameProvider
            implements SkipTosDialogPolicyListener.HistogramNameProvider {
        @Override
        public String getOnDeviceOwnedDetectedTimeHistogramName() {
            return mViewCreated ? "MobileFre.CctTos.IsDeviceOwnedCheckSpeed2.SlowerThanInflation"
                                : "MobileFre.CctTos.IsDeviceOwnedCheckSpeed2.FasterThanInflation";
        }

        @Override
        public String getOnPolicyAvailableTimeHistogramName() {
            return mViewCreated
                    ? "MobileFre.CctTos.EnterprisePolicyCheckSpeed2.SlowerThanInflation"
                    : "MobileFre.CctTos.EnterprisePolicyCheckSpeed2.FasterThanInflation";
        }
    };

    private boolean mViewCreated;
    private View mLoadingSpinnerContainer;
    private LoadingView mLoadingSpinner;
    private SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;
    private final OneshotSupplierImpl<PolicyService> mPolicyServiceProvider =
            new OneshotSupplierImpl<>();

    /** The {@link SystemClock} timestamp when onViewCreated is called. */
    private long mViewCreatedTimeMs;

    @Override
    public void onDestroy() {
        if (mLoadingSpinner != null) {
            mLoadingSpinner.destroy();
            mLoadingSpinner = null;
        }
        if (mSkipTosDialogPolicyListener != null) {
            mSkipTosDialogPolicyListener.destroy();
            mSkipTosDialogPolicyListener = null;
        }
        super.onDestroy();
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);

        // TODO(https://crbug.com/1143593): Replace FirstRunAppRestrictionInfo with a supplier.
        mSkipTosDialogPolicyListener = new SkipTosDialogPolicyListener(
                getPageDelegate().getFirstRunAppRestrictionInfo(), mPolicyServiceProvider,
                EnterpriseInfo.getInstance(), new CctTosFragmentMetricsNameProvider());
        mSkipTosDialogPolicyListener.onAvailable((b) -> onPolicyLoadListenerAvailable());
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mLoadingSpinnerContainer = view.findViewById(R.id.loading_view_container);
        mLoadingSpinner = view.findViewById(R.id.progress_spinner_large);
        mViewCreated = true;
        mViewCreatedTimeMs = SystemClock.elapsedRealtime();

        if (mSkipTosDialogPolicyListener.get() == null) {
            mLoadingSpinner.addObserver(this);
            mLoadingSpinner.showLoadingUI();
            setTosAndUmaVisible(false);
        } else if (mSkipTosDialogPolicyListener.get()) {
            // Skip the FRE if we know dialog is disabled by policy.
            setTosAndUmaVisible(false);
            exitCctFirstRun();
        }
    }

    @Override
    public void onNativeInitialized() {
        super.onNativeInitialized();
        if (mSkipTosDialogPolicyListener != null && mSkipTosDialogPolicyListener.get() == null) {
            mPolicyServiceProvider.set(PolicyServiceFactory.getGlobalPolicyService());
        }
    }

    @Override
    protected boolean canShowUmaCheckBox() {
        return super.canShowUmaCheckBox() && mSkipTosDialogPolicyListener.get() != null
                && !mSkipTosDialogPolicyListener.get();
    }

    @Override
    public void onShowLoadingUIComplete() {
        mLoadingSpinnerContainer.setVisibility(View.VISIBLE);
    }

    @Override
    public void onHideLoadingUIComplete() {
        assert mSkipTosDialogPolicyListener.get() != null;

        RecordHistogram.recordTimesHistogram("MobileFre.CctTos.LoadingDuration",
                SystemClock.elapsedRealtime() - mViewCreatedTimeMs);

        if (mSkipTosDialogPolicyListener.get()) {
            // TODO(crbug.com/1108564): Show the different UI that has the enterprise disclosure.
            exitCctFirstRun();
        } else {
            // Else, show the UMA as the loading spinner is GONE.
            boolean hasAccessibilityFocus = mLoadingSpinnerContainer.isAccessibilityFocused();
            mLoadingSpinnerContainer.setVisibility(View.GONE);
            setTosAndUmaVisible(true);

            if (hasAccessibilityFocus) {
                getToSAndPrivacyText().sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        }
    }

    private void onPolicyLoadListenerAvailable() {
        if (mViewCreated) mLoadingSpinner.hideLoadingUI();
    }

    private void exitCctFirstRun() {
        // TODO(crbug.com/1108564): Fire a signal to end this fragment when disclaimer is ready.
        // TODO(crbug.com/1108582): Save a shared pref indicating Enterprise CCT FRE is complete,
        //  and skip waiting for future cold starts.
        Log.d(TAG, "TosAndUmaFirstRunFragmentWithEnterpriseSupport finished.");
        getPageDelegate().exitFirstRun();
    }
}
