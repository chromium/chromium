// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.ProfileManagerUtils;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.policy.PolicyService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Base class for First Run Experience. */
// TODO(b/41493788): Consider renaming it now that it is also the base for the upgrade promo.
public abstract class FirstRunActivityBase extends AsyncInitializationActivity
        implements BackPressHandler {
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
    static final String SHOW_HISTORY_SYNC_PAGE = "ShowHistorySync";

    public static final boolean DEFAULT_METRICS_AND_CRASH_REPORTING = true;

    private static PolicyLoadListenerFactory sPolicyLoadListenerFactoryForTesting;

    private boolean mNativeInitialized;

    private final FirstRunAppRestrictionInfo mFirstRunAppRestrictionInfo;
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

    public FirstRunActivityBase() {
        mFirstRunAppRestrictionInfo = FirstRunAppRestrictionInfo.takeMaybeInitialized();
        mPolicyServiceSupplier = new OneshotSupplierImpl<>();
        mPolicyLoadListener =
                sPolicyLoadListenerFactoryForTesting == null
                        ? new PolicyLoadListener(
                                mFirstRunAppRestrictionInfo, mPolicyServiceSupplier)
                        : sPolicyLoadListenerFactoryForTesting.inject(
                                mFirstRunAppRestrictionInfo, mPolicyServiceSupplier);
        mStartTime = SystemClock.elapsedRealtime();
        mPolicyLoadListener.onAvailable(this::onPolicyLoadListenerAvailable);
    }

    protected long getStartTime() {
        return mStartTime;
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
        mChildAccountStatusSupplier =
                new ChildAccountStatusSupplier(accountManagerFacade, mFirstRunAppRestrictionInfo);

        // TODO(crbug.com/40939710): Find the underlying issue causing the status bar not to be set
        //  during FRE, this is just a temporary visual fix.
        if (BuildInfo.getInstance().isAutomotive) {
            StatusBarColorController.setStatusBarColor(getWindow(), Color.BLACK);
        }
    }

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        BackPressHelper.create(this, getOnBackPressedDispatcher(), this, getSecondaryActivity());
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
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher()) {
            @Nullable
            @Override
            protected OTRProfileID createOffTheRecordProfileID() {
                throw new IllegalStateException("Attempting to access incognito in the FRE");
            }
        };
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitialized = true;
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

    /** Called when back press is intercepted. */
    @Override
    public abstract @BackPressResult int handleBackPress();

    public abstract @SecondaryActivity int getSecondaryActivity();

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
        boolean pendingIntentIsCCT =
                IntentUtils.safeGetBooleanExtra(
                        getIntent(), EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (pendingIntent == null) return false;

        try {
            PendingIntent.OnFinished onFinished = null;
            if (pendingIntentIsCCT) {
                // After the PendingIntent has been sent, send a first run callback to custom tabs
                // if necessary.
                onFinished =
                        new PendingIntent.OnFinished() {
                            @Override
                            public void onSendFinished(
                                    PendingIntent pendingIntent,
                                    Intent intent,
                                    int resultCode,
                                    String resultData,
                                    Bundle resultExtras) {
                                // Use {@link FirstRunActivityBase#getIntent()} instead of {@link
                                // intent} parameter in order to use a more similar code path for
                                // completing first run and for aborting first run.
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

    /**
     * If the first run activity was triggered by a custom tab, notify app associated with
     * custom tab whether first run was completed.
     * @param freIntent First run activity intent.
     * @param complete  Whether first run completed successfully.
     */
    public static void notifyCustomTabCallbackFirstRunIfNecessary(
            Intent freIntent, boolean complete) {
        boolean launchedByCCT =
                IntentUtils.safeGetBooleanExtra(
                        freIntent, EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false);
        if (!launchedByCCT) return;

        Bundle launchIntentExtras =
                IntentUtils.safeGetBundleExtra(freIntent, EXTRA_CHROME_LAUNCH_INTENT_EXTRAS);
        CustomTabsConnection.getInstance()
                .sendFirstRunCallbackIfNecessary(launchIntentExtras, complete);
    }

    /**
     * Allows tests to inject a fake/mock {@link PolicyLoadListener} into {@link
     * FirstRunActivityBase}'s constructor.
     */
    public interface PolicyLoadListenerFactory {
        PolicyLoadListener inject(
                FirstRunAppRestrictionInfo appRestrictionInfo,
                OneshotSupplier<PolicyService> policyServiceSupplier);
    }

    /**
     * Forces the {@link FirstRunActivityBase}'s constructor to use a {@link PolicyLoadListener}
     * defined by a test, instead of creating its own instance.
     */
    public static void setPolicyLoadListenerFactoryForTesting(
            PolicyLoadListenerFactory policyLoadListenerFactory) {
        sPolicyLoadListenerFactoryForTesting = policyLoadListenerFactory;
        ResettersForTesting.register(() -> sPolicyLoadListenerFactoryForTesting = null);
    }
}
