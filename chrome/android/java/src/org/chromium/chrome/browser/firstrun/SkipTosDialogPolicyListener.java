// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.components.policy.PolicyService;

/**
 * Class that listens to signals related to ToSDialogBehavior. Supplies whether ToS dialog should be
 * skipped given policy settings.
 *
 * To be more specific:
 *  - Supplies [True] if the ToS dialog is not enabled by policy while device is fully managed;
 *  - Supplies [False] otherwise.
 */
class SkipTosDialogPolicyListener implements OneshotSupplier<Boolean> {
    /**
     * Interface that provides histogram to be recorded when signals are available in this listener.
     */
    interface HistogramNameProvider {
        /**
         * Name of time histogram to be recorded when signal "whether device is fully managed" is
         * available. The duration between creation of {@link SkipTosDialogPolicyListener} and
         * signal received will be recorded.
         *
         * @return Name of histogram to be recorded when signal is available.
         */
        String getOnDeviceOwnedDetectedTimeHistogramName();

        /**
         * Name of time histogram to be recorded when signal "whether the Tos dialog is enabled on
         * the device" is available. This histogram is not recorded when the value of policy
         * TosDialogBehavior is not used to determine the output of this listener.
         *
         * The duration between creation of {@link SkipTosDialogPolicyListener} and signal received
         * will be recorded.
         *
         * @return Name of histogram to be recorded when signal is available.
         */
        String getOnPolicyAvailableTimeHistogramName();
    }

    private final CallbackController mCallbackController;
    private final OneshotSupplierImpl<Boolean> mSkipTosDialogPolicySupplier;
    private final PolicyLoadListener mPolicyLoadListener;
    private final long mObjectCreatedTimeMs;

    private final @Nullable HistogramNameProvider mHistNameProvider;

    /**
     * The value of whether the ToS dialog is enabled on the device. If the value is false, it means
     * TosDialogBehavior policy is found and set to SKIP. This can be null when this information
     * is not ready yet.
     */
    private @Nullable Boolean mTosDialogEnabled;
    /**
     * Whether the current device is organization owned. This will start null before the check
     * occurs. The FRE can only be skipped if the device is organization owned.
     */
    private @Nullable Boolean mIsDeviceOwned;

    /**
     * @param firstRunAppRestrictionInfo Source that providers app restriction information.
     * @param policyServiceSupplier Supplier that providers PolicyService when native initialized.
     * @param enterpriseInfo Source that provides whether device is managed.
     * @param histogramNameProvider Provider that provides histogram names when signals are
     *         available.
     */
    public SkipTosDialogPolicyListener(FirstRunAppRestrictionInfo firstRunAppRestrictionInfo,
            OneshotSupplier<PolicyService> policyServiceSupplier, EnterpriseInfo enterpriseInfo,
            @Nullable HistogramNameProvider histogramNameProvider) {
        this(new PolicyLoadListener(firstRunAppRestrictionInfo, policyServiceSupplier),
                enterpriseInfo, histogramNameProvider);
    }

    @VisibleForTesting
    SkipTosDialogPolicyListener(PolicyLoadListener policyLoadListener,
            EnterpriseInfo enterpriseInfo, @Nullable HistogramNameProvider histogramNameProvider) {
        mObjectCreatedTimeMs = SystemClock.elapsedRealtime();
        mSkipTosDialogPolicySupplier = new OneshotSupplierImpl<>();
        mCallbackController = new CallbackController();
        mHistNameProvider = histogramNameProvider;
        mPolicyLoadListener = policyLoadListener;

        Boolean hasPolicy = mPolicyLoadListener.onAvailable(
                mCallbackController.makeCancelable(this::onPolicyLoadListenerAvailable));
        if (hasPolicy != null) {
            onPolicyLoadListenerAvailable(hasPolicy);
        }

        // Check EnterpriseInfo if still needed.
        if (mSkipTosDialogPolicySupplier.get() == null) {
            enterpriseInfo.getDeviceEnterpriseInfo(
                    mCallbackController.makeCancelable(this::onIsDeviceOwnedDetected));
        }
    }

    /**
     * Destroy the instance and remove all its dependencies.
     */
    public void destroy() {
        mCallbackController.destroy();
        mPolicyLoadListener.destroy();
    }

    @Override
    public Boolean onAvailable(Callback<Boolean> callback) {
        return mSkipTosDialogPolicySupplier.onAvailable(callback);
    }

    /**
     * @return Whether the ToS dialog should be skipped given settings on device.
     */
    @Override
    public Boolean get() {
        return mSkipTosDialogPolicySupplier.get();
    }

    private void onPolicyLoadListenerAvailable(boolean mightHavePolicy) {
        if (mTosDialogEnabled != null) return;

        if (!mightHavePolicy) {
            mTosDialogEnabled = true;
        } else {
            mTosDialogEnabled = FirstRunUtils.isCctTosDialogEnabled();
            if (mHistNameProvider != null) {
                String histogramOnPolicyLoaded =
                        mHistNameProvider.getOnPolicyAvailableTimeHistogramName();
                assert !TextUtils.isEmpty(histogramOnPolicyLoaded);
                RecordHistogram.recordTimesHistogram(histogramOnPolicyLoaded,
                        SystemClock.elapsedRealtime() - mObjectCreatedTimeMs);
            }
        }

        setSupplierIfDecidable();
    }

    private void onIsDeviceOwnedDetected(EnterpriseInfo.OwnedState ownedState) {
        if (mIsDeviceOwned != null) return;

        mIsDeviceOwned = ownedState != null && ownedState.mDeviceOwned;
        if (mHistNameProvider != null) {
            String histogramOnEnterpriseInfoLoaded =
                    mHistNameProvider.getOnDeviceOwnedDetectedTimeHistogramName();
            assert !TextUtils.isEmpty(histogramOnEnterpriseInfoLoaded);
            RecordHistogram.recordTimesHistogram(histogramOnEnterpriseInfoLoaded,
                    SystemClock.elapsedRealtime() - mObjectCreatedTimeMs);
        }

        setSupplierIfDecidable();
    }

    private void setSupplierIfDecidable() {
        if (mSkipTosDialogPolicySupplier.get() != null) return;

        boolean confirmedDeviceNotOwned = mIsDeviceOwned != null && !mIsDeviceOwned;
        boolean confirmedTosDialogEnabled = mTosDialogEnabled != null && mTosDialogEnabled;
        boolean hasOutstandingSignal = mIsDeviceOwned == null || mTosDialogEnabled == null;

        if (!hasOutstandingSignal) {
            mSkipTosDialogPolicySupplier.set(!mTosDialogEnabled && mIsDeviceOwned);
        } else if (confirmedTosDialogEnabled || confirmedDeviceNotOwned) {
            mSkipTosDialogPolicySupplier.set(false);
        }
    }
}
