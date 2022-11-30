// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.components.policy.PolicyService;
import org.chromium.ui.widget.LoadingView;

/**
 * Another FirstRunFragment that is only used when running with CCT.
 */
public class TosAndUmaFirstRunFragmentWithEnterpriseSupport
        extends ToSAndUMAFirstRunFragment implements LoadingView.Observer {
    private static final String TAG = "TosAndUmaFragment";

    private static Runnable sOverridenOnExitFreRunnableForTest;

    private class CctTosFragmentMetricsNameProvider
            implements SkipTosDialogPolicyListener.HistogramNameProvider {
        @Override
        public String getOnDeviceOwnedDetectedTimeHistogramName() {
            // Seems to currently be impossible to ever hit the faster case here.
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
    private View mBottomGroup;
    private View mLoadingSpinnerContainer;
    private LoadingView mLoadingSpinner;
    private TextView mPrivacyDisclaimer;
    private SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;
    private final OneshotSupplierImpl<PolicyService> mPolicyServiceProvider =
            new OneshotSupplierImpl<>();

    private Handler mHandler;

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
        if (mHandler != null) {
            // Remove all callback associated.
            mHandler.removeCallbacksAndMessages(null);
            mHandler = null;
        }
        super.onDestroy();
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);

        // TODO(https://crbug.com/1143593): Replace FirstRunAppRestrictionInfo with a supplier.
        mSkipTosDialogPolicyListener =
                new SkipTosDialogPolicyListener(getPageDelegate().getPolicyLoadListener(),
                        EnterpriseInfo.getInstance(), new CctTosFragmentMetricsNameProvider());
        mSkipTosDialogPolicyListener.onAvailable((ignored) -> onPolicyLoadListenerAvailable());
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mBottomGroup = view.findViewById(R.id.fre_bottom_group);
        mLoadingSpinnerContainer = view.findViewById(R.id.loading_view_container);
        mLoadingSpinner = view.findViewById(R.id.progress_spinner_large);
        mPrivacyDisclaimer = view.findViewById(R.id.privacy_disclaimer);
        mViewCreated = true;
        mViewCreatedTimeMs = SystemClock.elapsedRealtime();

        if (mSkipTosDialogPolicyListener.get() == null) {
            mLoadingSpinner.addObserver(this);
            mLoadingSpinner.showLoadingUI();
            mBottomGroup.setVisibility(View.GONE);
            setTosAndUmaVisible(false);
        } else if (mSkipTosDialogPolicyListener.get()) {
            // Skip the FRE if we know dialog is disabled by policy.
            mBottomGroup.setVisibility(View.GONE);
            setTosAndUmaVisible(false);
            exitCctFirstRun(/*shiftA11yFocus*/ false);
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
        super.onHideLoadingUIComplete();
        assert mSkipTosDialogPolicyListener.get() != null;

        RecordHistogram.recordTimesHistogram("MobileFre.CctTos.LoadingDuration",
                SystemClock.elapsedRealtime() - mViewCreatedTimeMs);

        boolean hasAccessibilityFocus = mLoadingSpinnerContainer.isAccessibilityFocused();
        mLoadingSpinnerContainer.setVisibility(View.GONE);
        if (mSkipTosDialogPolicyListener.get()) {
            exitCctFirstRun(hasAccessibilityFocus);
        } else {
            // Else, show the UMA as the loading spinner is GONE.
            mBottomGroup.setVisibility(View.VISIBLE);
            setTosAndUmaVisible(true);

            if (hasAccessibilityFocus) {
                getToSAndPrivacyText().sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        }
    }

    private void onPolicyLoadListenerAvailable() {
        if (mViewCreated) mLoadingSpinner.hideLoadingUI();
    }

    private void exitCctFirstRun(boolean shiftA11yFocus) {
        Log.d(TAG, "TosAndUmaFirstRunFragmentWithEnterpriseSupport finished.");
        mPrivacyDisclaimer.setVisibility(View.VISIBLE);

        // If the screen reader focus was on the loading spinner, to avoid the focus get lost from
        // the screen, shift the focus to the disclaimer instead. Otherwise, announce the disclaimer
        // without shifting the focus as it is not necessary.
        if (shiftA11yFocus) {
            mPrivacyDisclaimer.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        } else {
            mPrivacyDisclaimer.announceForAccessibility(mPrivacyDisclaimer.getText());
        }

        // Make sure this function is called at most once by asserting no handler is created yet.
        assert mHandler == null;
        Runnable exitFreRunnable = sOverridenOnExitFreRunnableForTest != null
                ? sOverridenOnExitFreRunnableForTest
                : () -> getPageDelegate().exitFirstRun();
        mHandler = new Handler(ThreadUtils.getUiThreadLooper());
        mHandler.postDelayed(exitFreRunnable, FirstRunUtils.getSkipTosExitDelayMs());
    }

    @VisibleForTesting
    static void setOverrideOnExitFreRunnableForTest(Runnable runnable) {
        sOverridenOnExitFreRunnableForTest = runnable;
    }
}
