// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.BackPressHelper;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.policy.PolicyService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Base class for First Run Experience. */
public abstract class FirstRunActivityBase
        extends AsyncInitializationActivity implements BackPressHandler {
    private static final String TAG = "FirstRunActivity";

    public static final String EXTRA_COMING_FROM_CHROME_ICON = "Extra.ComingFromChromeIcon";
    public static final String EXTRA_CHROME_LAUNCH_INTENT_IS_CCT =
            "Extra.FreChromeLaunchIntentIsCct";
    public static final String EXTRA_FRE_INTENT_CREATION_ELAPSED_REALTIME_MS =
            "Extra.FreIntentCreationElapsedRealtimeMs";

    // The intent to send once the FRE completes.
    public static final String EXTRA_FRE_COMPLETE_LAUNCH_INTENT = "Extra.FreChromeLaunchIntent";

    // The extras on the intent which initiated first run. (e.g. the extras on the intent
    // received by ChromeLauncherActivity.)
    public static final String EXTRA_CHROME_LAUNCH_INTENT_EXTRAS =
            "Extra.FreChromeLaunchIntentExtras";
    static final String SHOW_SEARCH_ENGINE_PAGE = "ShowSearchEnginePage";
    static final String SHOW_SYNC_CONSENT_PAGE = "ShowSyncConsent";

    public static final boolean DEFAULT_METRICS_AND_CRASH_REPORTING = true;

    private static PolicyLoadListenerFactory sPolicyLoadListenerFactory;

    private boolean mNativeInitialized;

    private final FirstRunAppRestrictionInfo mFirstRunAppRestrictionInfo;
    private final OneshotSupplierImpl<PolicyService> mPolicyServiceSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>() {
                // Always intercept back press.
                { set(true); }
            };
    private PolicyLoadListener mPolicyLoadListener;

    private final long mStartTime;
    private long mNativeInitializedTime;

    private ChildAccountStatusSupplier mChildAccountStatusSupplier;

    public FirstRunActivityBase() {
        mFirstRunAppRestrictionInfo = FirstRunAppRestrictionInfo.takeMaybeInitialized();
        mPolicyServiceSupplier = new OneshotSupplierImpl<>();
        mPolicyLoadListener = sPolicyLoadListenerFactory == null
                ? new PolicyLoadListener(mFirstRunAppRestrictionInfo, mPolicyServiceSupplier)
                : sPolicyLoadListenerFactory.inject(
                        mFirstRunAppRestrictionInfo, mPolicyServiceSupplier);
        mStartTime = SystemClock.elapsedRealtime();
        mPolicyLoadListener.onAvailable(this::onPolicyLoadListenerAvailable);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // The user is already in First Run.
        return false;
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    @CallSuper
    public void triggerLayoutInflation() {
        AccountManagerFacade accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        if (FREMobileIdentityConsistencyFieldTrial.isEnabled()) {
            mChildAccountStatusSupplier = new ChildAccountStatusSupplier(
                    accountManagerFacade, mFirstRunAppRestrictionInfo);
        } else {
            mChildAccountStatusSupplier =
                    new ChildAccountStatusSupplier(accountManagerFacade, null);
        }
    }

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        if (BackPressManager.isSecondaryActivityEnabled()) {
            BackPressHelper.create(this, getOnBackPressedDispatcher(), this);
        } else {
            BackPressHelper.create(this, getOnBackPressedDispatcher(), () -> {
                handleBackPress();
                return true;
            });
        }
    }

    // Activity:
    @Override
    public void onPause() {
        super.onPause();
        // As with onResume() below, for historical reasons the FRE has been able to report
        // background time before post-native initialization, unlike other activities. See
        // http://crrev.com/436530.
        UmaUtils.recordBackgroundTimeWithNative();
        flushPersistentData();
    }

    @Override
    public void onResume() {
        SimpleStartupForegroundSessionDetector.discardSession();
        super.onResume();
        // Since the FRE may be shown before any tab is shown, mark that this is the point at which
        // Chrome went to foreground. Other activities can only
        // recordForegroundStartTimeWithNative() after the post-native initialization has started.
        // See http://crrev.com/436530.
        UmaUtils.recordForegroundStartTimeWithNative();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitialized = true;
        mNativeInitializedTime = SystemClock.elapsedRealtime();
        RecordHistogram.recordTimesHistogram(
                "MobileFre.NativeInitialized", mNativeInitializedTime - mStartTime);
        mPolicyServiceSupplier.set(PolicyServiceFactory.getGlobalPolicyService());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mPolicyLoadListener.destroy();
        mFirstRunAppRestrictionInfo.destroy();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /**
     * Called when back press is intercepted.
     */
    @Override
    public abstract @BackPressResult int handleBackPress();

    protected void flushPersistentData() {
        if (mNativeInitialized) {
            ProfileManagerUtils.flushPersistentDataForAllProfiles();
        }
    }

    /**
     * Sends PendingIntent included with the EXTRA_FRE_COMPLETE_LAUNCH_INTENT extra.
     * @return Whether a pending intent was sent.
     */
    protected final boolean sendFirstRunCompletePendingIntent() {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelableExtra(getIntent(), EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        boolean pendingIntentIsCCT = IntentUtils.safeGetBooleanExtra(
                getIntent(), EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (pendingIntent == null) return false;

        try {
            PendingIntent.OnFinished onFinished = null;
            if (pendingIntentIsCCT) {
                // After the PendingIntent has been sent, send a first run callback to custom tabs
                // if necessary.
                onFinished = new PendingIntent.OnFinished() {
                    @Override
                    public void onSendFinished(PendingIntent pendingIntent, Intent intent,
                            int resultCode, String resultData, Bundle resultExtras) {
                        // Use {@link FirstRunActivityBase#getIntent()} instead of {@link intent}
                        // parameter in order to use a more similar code path for completing first
                        // run and for aborting first run.
                        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), true);
                    }
                };
            }

            // Use the PendingIntent to send the intent that originally launched Chrome. The intent
            // will go back to the ChromeLauncherActivity, which will route it accordingly.
            pendingIntent.send(Activity.RESULT_OK, onFinished, null);
            // Use fade-out animation for the transition from this activity so the original intent.
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            return true;
        } catch (CanceledException e) {
            Log.e(TAG, "Unable to send PendingIntent.", e);
        }
        return false;
    }

    protected FirstRunAppRestrictionInfo getFirstRunAppRestrictionInfo() {
        return mFirstRunAppRestrictionInfo;
    }

    protected void onPolicyLoadListenerAvailable(boolean onDevicePolicyFound) {
        if (!mNativeInitialized) return;

        assert mNativeInitializedTime != 0;
        long delayAfterNative = SystemClock.elapsedRealtime() - mNativeInitializedTime;
        String histogramName = onDevicePolicyFound
                ? "MobileFre.PolicyServiceInitDelayAfterNative.WithPolicy2"
                : "MobileFre.PolicyServiceInitDelayAfterNative.WithoutPolicy2";
        RecordHistogram.recordTimesHistogram(histogramName, delayAfterNative);
    }

    /**
     * @return PolicyLoadListener used to indicate if policy initialization is complete.
     * @see PolicyLoadListener for return value expectation.
     */
    public OneshotSupplier<Boolean> getPolicyLoadListener() {
      return mPolicyLoadListener;
    }

    /**
     * Returns the supplier that supplies child account status.
     */
    public OneshotSupplier<Boolean> getChildAccountStatusSupplier() {
        return mChildAccountStatusSupplier;
    }

    /**
     * If the first run activity was triggered by a custom tab, notify app associated with
     * custom tab whether first run was completed.
     * @param freIntent First run activity intent.
     * @param complete  Whether first run completed successfully.
     */
    public static void notifyCustomTabCallbackFirstRunIfNecessary(
            Intent freIntent, boolean complete) {
        boolean launchedByCCT = IntentUtils.safeGetBooleanExtra(
                freIntent, EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (!launchedByCCT) return;

        Bundle launchIntentExtras =
                IntentUtils.safeGetBundleExtra(freIntent, EXTRA_CHROME_LAUNCH_INTENT_EXTRAS);
        CustomTabsConnection.getInstance().sendFirstRunCallbackIfNecessary(
                launchIntentExtras, complete);
    }

    /**
     * Allows tests to inject a fake/mock {@link PolicyLoadListener} into {@link
     * FirstRunActivityBase}'s constructor.
     */
    public interface PolicyLoadListenerFactory {
        PolicyLoadListener inject(FirstRunAppRestrictionInfo appRestrictionInfo,
                OneshotSupplier<PolicyService> policyServiceSupplier);
    }

    /**
     * Forces the {@link FirstRunActivityBase}'s constructor to use a {@link PolicyLoadListener}
     * defined by a test, instead of creating its own instance.
     */
    @VisibleForTesting
    public static void setPolicyLoadListenerFactoryForTesting(
            PolicyLoadListenerFactory policyLoadListenerFactory) {
        sPolicyLoadListenerFactory = policyLoadListenerFactory;
    }
}
