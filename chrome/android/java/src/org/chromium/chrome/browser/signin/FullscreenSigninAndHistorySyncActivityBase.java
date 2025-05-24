// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.graphics.Color;
import android.os.SystemClock;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.policy.PolicyService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Base class for First run experience and fullscreen signin and history sync promos. */
public abstract class FullscreenSigninAndHistorySyncActivityBase extends AsyncInitializationActivity
        implements BackPressHandler {
    private final AppRestrictionSupplier mAppRestrictionSupplier;
    private final OneshotSupplierImpl<PolicyService> mPolicyServiceSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>() {
                // Always intercept back press.
                {
                    set(true);
                }
            };
    private final PolicyLoadListener mPolicyLoadListener;
    private final long mStartTime;

    private ChildAccountStatusSupplier mChildAccountStatusSupplier;

    public FullscreenSigninAndHistorySyncActivityBase() {
        mAppRestrictionSupplier = AppRestrictionSupplier.takeMaybeInitialized();
        mPolicyServiceSupplier = new OneshotSupplierImpl<>();
        mPolicyLoadListener =
                new PolicyLoadListener(mAppRestrictionSupplier, mPolicyServiceSupplier);
        mPolicyLoadListener.onAvailable(this::onPolicyLoadListenerAvailable);
        mStartTime = SystemClock.elapsedRealtime();
    }

    /** Returns the elapsed time at which the activity was started, in milliseconds. */
    protected long getStartTime() {
        return mStartTime;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher()) {
            @Nullable
            @Override
            protected OtrProfileId createOffTheRecordProfileId() {
                throw new IllegalStateException("Attempting to access incognito in the re-FRE");
            }
        };
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mPolicyServiceSupplier.set(PolicyServiceFactory.getGlobalPolicyService());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mPolicyLoadListener.destroy();
        mAppRestrictionSupplier.destroy();
    }

    @Override
    @CallSuper
    public void triggerLayoutInflation() {
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mChildAccountStatusSupplier =
                new ChildAccountStatusSupplier(accountManagerFacade, mAppRestrictionSupplier);

        // TODO(crbug.com/40939710): Find the underlying issue causing the status bar not to be set
        //  during re-FRE, this is just a temporary visual fix.
        if (BuildInfo.getInstance().isAutomotive) {
            StatusBarColorController.setStatusBarColor(
                    (getEdgeToEdgeManager() != null)
                            ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                            : null,
                    getWindow(),
                    Color.BLACK);
        }
    }

    /** Observer method for the policy load listener. Overridden by inheriting classes. */
    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {}

    /**
     * @return PolicyLoadListener used to indicate if policy initialization is complete.
     * @see PolicyLoadListener for return value expectation.
     */
    public OneshotSupplier<Boolean> getPolicyLoadListener() {
        return mPolicyLoadListener;
    }

    /** Returns the supplier that supplies child account status. */
    public OneshotSupplier<Boolean> getChildAccountStatusSupplier() {
        return mChildAccountStatusSupplier;
    }

    protected AppRestrictionSupplier getAppRestrictionSupplier() {
        return mAppRestrictionSupplier;
    }

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        BackPressHelper.create(this, getOnBackPressedDispatcher(), this);
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /** Called when back press is intercepted. */
    @Override
    public abstract @BackPressResult int handleBackPress();
}
