// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ChildAccountStatus;

import java.util.List;

/**
 * A helper to determine what should be the sequence of First Run Experience screens, and whether
 * it should be run.
 *
 * Usage:
 * new FirstRunFlowSequencer(activity, launcherProvidedProperties) {
 *     override onFlowIsKnown
 * }.start();
 */
public abstract class FirstRunFlowSequencer  {
    private static final String TAG = "firstrun";

    private final Activity mActivity;
    private @ChildAccountStatus.Status int mChildAccountStatus;
    private List<Account> mGoogleAccounts;

    /**
     * Callback that is called once the flow is determined.
     * If the properties is null, the First Run experience needs to finish and
     * restart the original intent if necessary.
     * @param freProperties Properties to be used in the First Run activity, or null.
     */
    public abstract void onFlowIsKnown(Bundle freProperties);

    public FirstRunFlowSequencer(Activity activity) {
        mActivity = activity;
    }

    /**
     * Starts determining parameters for the First Run.
     * Once finished, calls onFlowIsKnown().
     */
    public void start() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
                || ApiCompatibilityUtils.isDemoUser()
                || ApiCompatibilityUtils.isRunningInUserTestHarness()) {
            onFlowIsKnown(null);
            return;
        }

        long childAccountStatusStart = SystemClock.elapsedRealtime();
        ChildAccountService.checkChildAccountStatus(status -> {
            RecordHistogram.recordTimesHistogram("MobileFre.ChildAccountStatusDuration",
                    SystemClock.elapsedRealtime() - childAccountStatusStart);
            initializeSharedState(status);
            processFreEnvironmentPreNative();
        });
    }

    @VisibleForTesting
    protected boolean isFirstRunFlowComplete() {
        return FirstRunStatus.getFirstRunFlowComplete();
    }

    @VisibleForTesting
    protected boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount();
    }

    @VisibleForTesting
    protected boolean isSyncAllowed() {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        return FirstRunUtils.canAllowSync() && !signinManager.isSigninDisabledByPolicy()
                && signinManager.isSigninSupported();
    }

    @VisibleForTesting
    protected List<Account> getGoogleAccounts() {
        return AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts();
    }

    @VisibleForTesting
    protected boolean shouldSkipFirstUseHints() {
        return Settings.Secure.getInt(
                       mActivity.getContentResolver(), Settings.Secure.SKIP_FIRST_USE_HINTS, 0)
                != 0;
    }

    @VisibleForTesting
    protected boolean isFirstRunEulaAccepted() {
        return FirstRunUtils.isFirstRunEulaAccepted();
    }

    protected boolean shouldShowDataReductionPage() {
        return !DataReductionProxySettings.getInstance().isDataReductionProxyManaged()
                && DataReductionProxySettings.getInstance().isDataReductionProxyFREPromoAllowed();
    }

    @VisibleForTesting
    protected boolean shouldShowSearchEnginePage() {
        @LocaleManager.SearchEnginePromoType
        int searchPromoType = LocaleManager.getInstance().getSearchEnginePromoShowType();
        return searchPromoType == LocaleManager.SearchEnginePromoType.SHOW_NEW
                || searchPromoType == LocaleManager.SearchEnginePromoType.SHOW_EXISTING;
    }

    @VisibleForTesting
    protected void setFirstRunFlowSignInComplete() {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(true);
    }

    void initializeSharedState(@ChildAccountStatus.Status int childAccountStatus) {
        mChildAccountStatus = childAccountStatus;
        // Note that fetching accounts isn't instrumented because it's typically very fast. It seems
        // fetching child account status causes accounts to be cached. Revisit this if the ordering
        // of these calls is changed.
        mGoogleAccounts = getGoogleAccounts();
    }

    void processFreEnvironmentPreNative() {
        if (isFirstRunFlowComplete()) {
            assert isFirstRunEulaAccepted();
            // We do not need any interactive FRE.
            onFlowIsKnown(null);
            return;
        }

        Bundle freProperties = new Bundle();
        freProperties.putInt(SyncConsentFirstRunFragment.CHILD_ACCOUNT_STATUS, mChildAccountStatus);

        onFlowIsKnown(freProperties);
        if (ChildAccountStatus.isChild(mChildAccountStatus)) {
            setFirstRunFlowSignInComplete();
        }
    }

    /**
     * Will be called either when policies are initialized, or when native is initialized if we have
     * no on-device policies.
     * @param freProperties Resulting FRE properties bundle.
     */
    public void onNativeAndPoliciesInitialized(Bundle freProperties) {
        // We show the sign-in page if sync is allowed, and not signed in, and
        // - no "skip the first use hints" is set, or
        // - "skip the first use hints" is set, but there is at least one account.
        boolean offerSignInOk = isSyncAllowed() && !isSignedIn()
                && (!shouldSkipFirstUseHints() || !mGoogleAccounts.isEmpty());
        freProperties.putBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE, offerSignInOk);
        freProperties.putBoolean(
                FirstRunActivity.SHOW_DATA_REDUCTION_PAGE, shouldShowDataReductionPage());
        freProperties.putBoolean(
                FirstRunActivity.SHOW_SEARCH_ENGINE_PAGE, shouldShowSearchEnginePage());
    }

    /**
     * Marks a given flow as completed.
     * @param signInAccountName The account name for the pending sign-in request. (Or null)
     * @param showSignInSettings Whether the user selected to see the settings once signed in.
     */
    public static void markFlowAsCompleted(String signInAccountName, boolean showSignInSettings) {
        // When the user accepts ToS in the Setup Wizard, we do not show the ToS page to the user
        // because the user has already accepted one outside FRE.
        if (!FirstRunUtils.isFirstRunEulaAccepted()) {
            FirstRunUtils.setEulaAccepted();
        }

        // Mark the FRE flow as complete and set the sign-in flow preferences if necessary.
        FirstRunSignInProcessor.finalizeFirstRunFlowState(signInAccountName, showSignInSettings);
    }

    /**
     * Checks if the First Run Experience needs to be launched.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @param fromIntent Intent used to launch the caller.
     * @return Whether the First Run Experience needs to be launched.
     */
    public static boolean checkIfFirstRunIsNecessary(
            boolean preferLightweightFre, Intent fromIntent) {
        boolean isCct = fromIntent.getBooleanExtra(
                                FirstRunActivityBase.EXTRA_CHROME_LAUNCH_INTENT_IS_CCT, false)
                || LaunchIntentDispatcher.isCustomTabIntent(fromIntent);
        return checkIfFirstRunIsNecessary(preferLightweightFre, isCct);
    }

    /**
     * Checks if the First Run Experience needs to be launched.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @param isCct Whether this check is being made in the context of a CCT.
     * @return Whether the First Run Experience needs to be launched.
     */
    public static boolean checkIfFirstRunIsNecessary(boolean preferLightweightFre, boolean isCct) {
        // If FRE is disabled (e.g. in tests), proceed directly to the intent handling.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
                || ApiCompatibilityUtils.isDemoUser()
                || ApiCompatibilityUtils.isRunningInUserTestHarness()) {
            return false;
        }
        if (FirstRunStatus.getFirstRunFlowComplete()) {
            // Promo pages are removed, so there is nothing else to show in FRE.
            return false;
        }
        if (FirstRunStatus.isFirstRunSkippedByPolicy() && (isCct || preferLightweightFre)) {
            // Domain policies may have caused CCTs to skip the FRE. While this needs to be figured
            // out at runtime for each app restart, it should apply to all CCTs for the duration of
            // the app's lifetime.
            return false;
        }
        if (preferLightweightFre
                && (FirstRunStatus.shouldSkipWelcomePage()
                        || FirstRunStatus.getLightweightFirstRunFlowComplete())) {
            return false;
        }
        return true;
    }

    /**
     * Tries to launch the First Run Experience.  If the Activity was launched with the wrong Intent
     * flags, we first relaunch it to make sure it runs in its own task, then trigger First Run.
     *
     * @param caller               Activity instance that is checking if first run is necessary.
     * @param fromIntent           Intent used to launch the caller.
     * @param requiresBroadcast    Whether or not the Intent triggers a BroadcastReceiver.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @return Whether startup must be blocked (e.g. via Activity#finish or dropping the Intent).
     */
    public static boolean launch(Context caller, Intent fromIntent, boolean requiresBroadcast,
            boolean preferLightweightFre) {
        // Check if the user needs to go through First Run at all.
        if (!checkIfFirstRunIsNecessary(preferLightweightFre, fromIntent)) return false;

        String intentUrl = IntentHandler.getUrlFromIntent(fromIntent);
        Uri uri = intentUrl != null ? Uri.parse(intentUrl) : null;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            caller.grantUriPermission(
                    caller.getPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        Log.d(TAG, "Redirecting user through FRE.");

        // Launch the async restriction checking as soon as we know we'll be running FRE.
        FirstRunAppRestrictionInfo.startInitializationHint();

        if ((fromIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
            FreIntentCreator intentCreator = new FreIntentCreator();
            Intent freIntent = intentCreator.create(
                    caller, fromIntent, requiresBroadcast, preferLightweightFre);

            // Although the FRE tries to run in the same task now, this is still needed for
            // non-activity entry points like the search widget to launch at all. This flag does not
            // seem to preclude an old task from being reused.
            if (!(caller instanceof Activity)) {
                freIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }

            boolean isVrIntent = VrModuleProvider.getIntentDelegate().isVrIntent(fromIntent);
            if (isVrIntent) {
                freIntent =
                        VrModuleProvider.getIntentDelegate().setupVrFreIntent(caller, freIntent);
                // We cannot access Chrome right now, e.g. because the VR module is not installed.
                if (freIntent == null) return true;
            }
            IntentUtils.safeStartActivity(caller, freIntent);
        } else {
            // First Run requires that the Intent contains NEW_TASK so that it doesn't sit on top
            // of something else.
            Intent newIntent = new Intent(fromIntent);
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(caller, newIntent);
        }
        return true;
    }
}
