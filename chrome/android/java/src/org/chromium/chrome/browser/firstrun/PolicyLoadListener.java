// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.components.policy.PolicyService;
import org.chromium.components.policy.PolicyService.Observer;

/**
 * Class that is responsible for listening to signals before policy service is fully initialized in
 * C++. The value it supplies will be ready when a decision about *whether reading policy value is
 * necessary*.
 *
 * The signals this class observes are policy service initialization and Android app restrictions.
 * If no app restrictions are found before the policy service is initialized, early out of the
 * loading process and inform the listeners.
 *
 * To be more specific:
 *
 *  - Supplies [True] if policy service is initialized and policy might be applied;
 *  - Supplies [False] if no app restriction is found, thus no polices will be found on device.
 */
public class PolicyLoadListener implements OneshotSupplier<Boolean> {
    private static final String TAG = "PolicyLoadListener";

    private final CallbackController mCallbackController;
    private final OneshotSupplierImpl<Boolean> mMightHavePoliciesSupplier;
    private final OneshotSupplier<PolicyService> mPolicyServiceSupplier;

    private PolicyService.Observer mPolicyServiceObserver;

    /**
     * Whether app restriction is found on the device. This can be null when this information is not
     * ready yet.
     */
    private @Nullable Boolean mHasRestriction;

    @VisibleForTesting
    public PolicyLoadListener() {
        mCallbackController = null;
        mMightHavePoliciesSupplier = null;
        mPolicyServiceSupplier = null;
    }

    /**
     * Create the instance and start listening to signals from policy service and app restrictions.
     * @param appRestrictionInfo Class that provides whether app restriction is found on device.
     * @param policyServiceSupplier Supplier of PolicyService that this class listened to.
     */
    public PolicyLoadListener(
            FirstRunAppRestrictionInfo appRestrictionInfo,
            OneshotSupplier<PolicyService> policyServiceSupplier) {
        mCallbackController = new CallbackController();
        mMightHavePoliciesSupplier = new OneshotSupplierImpl<>();

        mPolicyServiceSupplier = policyServiceSupplier;

        appRestrictionInfo.getHasAppRestriction(
                mCallbackController.makeCancelable(this::onAppRestrictionDetected));
        policyServiceSupplier.onAvailable(
                mCallbackController.makeCancelable(this::onPolicyServiceAvailable));
    }

    /** Cancel all unfinished callback and remove observer for policy service if any. */
    public void destroy() {
        mCallbackController.destroy();
        if (mPolicyServiceObserver != null) {
            mPolicyServiceSupplier.get().removeObserver(mPolicyServiceObserver);
            mPolicyServiceObserver = null;
        }
    }

    @Override
    public Boolean onAvailable(Callback<Boolean> callback) {
        return mMightHavePoliciesSupplier.onAvailable(mCallbackController.makeCancelable(callback));
    }

    @Override
    public Boolean get() {
        return mMightHavePoliciesSupplier.get();
    }

    /** Check status for internal signals. If loading completes, mark loading is finished. */
    private void setSupplierIfDecidable() {
        // Early return if policy value has been set.
        if (mMightHavePoliciesSupplier.get() != null) return;

        boolean confirmedNoAppRestriction = mHasRestriction != null && !mHasRestriction;
        boolean policyServiceInitialized =
                (mPolicyServiceSupplier.get() != null
                        && mPolicyServiceSupplier.get().isInitializationComplete());
        Log.i(
                TAG,
                "#setSupplierIfDecidable() confirmedNoAppRestriction:"
                        + confirmedNoAppRestriction
                        + " policyServiceInitialized:"
                        + policyServiceInitialized);
        if (confirmedNoAppRestriction) {
            // No app restriction is found.
            mMightHavePoliciesSupplier.set(false);
        } else if (policyServiceInitialized) {
            // Policies are ready to be read,
            mMightHavePoliciesSupplier.set(true);
        }
    }

    private void onAppRestrictionDetected(boolean hasAppRestriction) {
        mHasRestriction = hasAppRestriction;
        setSupplierIfDecidable();
    }

    private void onPolicyServiceAvailable(PolicyService policyService) {
        Log.i(TAG, "#onPolicyServiceAvailable() " + policyService.isInitializationComplete());

        // Ignore the signal if loading is no longer necessary.
        if (mMightHavePoliciesSupplier.get() != null) return;

        assert policyService != null;

        if (policyService.isInitializationComplete()) {
            setSupplierIfDecidable();
        } else {
            mPolicyServiceObserver =
                    new Observer() {
                        @Override
                        public void onPolicyServiceInitialized() {
                            Log.i(
                                    TAG,
                                    "#onPolicyServiceInitialized() "
                                            + policyService.isInitializationComplete());
                            policyService.removeObserver(mPolicyServiceObserver);
                            mPolicyServiceObserver = null;
                            setSupplierIfDecidable();
                        }
                    };
            policyService.addObserver(mPolicyServiceObserver);
        }
    }
}
