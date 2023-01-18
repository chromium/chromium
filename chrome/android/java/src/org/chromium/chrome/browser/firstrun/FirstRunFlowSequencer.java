// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

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

    /**
     * A delegate class to determine if first run promo pages should be shown based on various
     * signals. Some methods may be overridden by tests to fake desired behavior.
     */
    @VisibleForTesting
    public static class FirstRunFlowSequencerDelegate {
        /** Returns true if the sync consent promo page should be shown. */
        boolean shouldShowSyncConsentPage(
                Activity activity, List<Account> accounts, boolean isChild) {
            if (isChild) {
                // Always show the sync consent page for child account.
                return true;
            }
            final IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(
                            Profile.getLastUsedRegularProfile());
            if (identityManager.hasPrimaryAccount(ConsentLevel.SYNC) || !isSyncAllowed()) {
                // No need to show the sync consent page if users already consented to sync or
                // if sync is not allowed.
                return false;
            }
            if (FREMobileIdentityConsistencyFieldTrial.isEnabled()) {
                // Show the sync consent page only to the signed-in users.
                return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
            } else {
                // We show the sync consent page if sync is allowed, and not signed in, and
                // - "skip the first use hints" is not set, or
                // - "skip the first use hints" is set, but there is at least one account.
                return !shouldSkipFirstUseHints(activity) || !accounts.isEmpty();
            }
        }

        /** @return true if the Search Engine promo page should be shown. */
        @VisibleForTesting
        public boolean shouldShowSearchEnginePage() {
            @SearchEnginePromoType
            int searchPromoType = LocaleManager.getInstance().getSearchEnginePromoShowType();
            return searchPromoType == SearchEnginePromoType.SHOW_NEW
                    || searchPromoType == SearchEnginePromoType.SHOW_EXISTING;
        }

        /** @return true if Sync is allowed for the current user. */
        @VisibleForTesting
        protected boolean isSyncAllowed() {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                    Profile.getLastUsedRegularProfile());
            return FirstRunUtils.canAllowSync() && !signinManager.isSigninDisabledByPolicy()
                    && signinManager.isSigninSupported();
        }

        /** @return true if first use hints should be skipped. */
        @VisibleForTesting
        protected boolean shouldSkipFirstUseHints(Activity activity) {
            return Settings.Secure.getInt(
                           activity.getContentResolver(), Settings.Secure.SKIP_FIRST_USE_HINTS, 0)
                    != 0;
        }
    }

    private final Activity mActivity;
    /**
     * The delegate to be used by the Sequencer. By default, it's an instance of
     * {@link FirstRunFlowSequencerDelegate}, unless it's overridden by {@code sDelegateForTesting}.
     */
    private FirstRunFlowSequencerDelegate mDelegate;

    /** If not null, overrides {@code mDelegate} for this object during tests. */
    private static FirstRunFlowSequencerDelegate sDelegateForTesting;

    private boolean mIsFlowKnown;
    private Boolean mIsChild;
    private List<Account> mGoogleAccounts;

    /**
     * Callback that is called once the flow is determined.
     * If the properties is null, the First Run experience needs to finish and
     * restart the original intent if necessary.
     * @param freProperties Properties to be used in the First Run activity, or null.
     */
    public abstract void onFlowIsKnown(Bundle freProperties);

    public FirstRunFlowSequencer(
            Activity activity, OneshotSupplier<Boolean> childAccountStatusSupplier) {
        mActivity = activity;

        mDelegate = sDelegateForTesting != null ? sDelegateForTesting
                                                : new FirstRunFlowSequencerDelegate();

        childAccountStatusSupplier.onAvailable(this::setChildAccountStatus);
    }

    /**
     * Starts determining parameters for the First Run.
     * Once finished, calls onFlowIsKnown().
     *
     * TODO(https://crbug.com/1320487): Add Supplier to AccountManagerFacadeProvider and remove this
     *                                  method.
     */
    void start() {
        AccountManagerFacadeProvider.getInstance().getAccounts().then(accounts -> {
            RecordHistogram.recordCount1MHistogram(
                    "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE",
                    Math.min(accounts.size(), 2));
            setAccountList(accounts);
        });
    }

    @VisibleForTesting
    protected boolean shouldShowSearchEnginePage() {
        return mDelegate.shouldShowSearchEnginePage();
    }

    private boolean shouldShowSyncConsentPage() {
        return mDelegate.shouldShowSyncConsentPage(mActivity, mGoogleAccounts, mIsChild);
    }

    private void setChildAccountStatus(boolean isChild) {
        assert mIsChild == null;
        mIsChild = isChild;
        maybeProcessFreEnvironmentPreNative();
    }

    private void setAccountList(List<Account> accounts) {
        assert mGoogleAccounts == null && accounts != null;
        mGoogleAccounts = accounts;
        maybeProcessFreEnvironmentPreNative();
    }

    private void maybeProcessFreEnvironmentPreNative() {
        // Wait till both child account status and the list of accounts are available.
        if (mIsChild == null || mGoogleAccounts == null) return;

        if (mIsFlowKnown) return;
        mIsFlowKnown = true;

        Bundle freProperties = new Bundle();
        freProperties.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, mIsChild);

        onFlowIsKnown(freProperties);
    }

    /**
     * Will be called either when policies are initialized, or when native is initialized if we have
     * no on-device policies.
     * @param freProperties Resulting FRE properties bundle.
     */
    public void updateFirstRunProperties(Bundle freProperties) {
        freProperties.putBoolean(
                FirstRunActivity.SHOW_SYNC_CONSENT_PAGE, shouldShowSyncConsentPage());
        freProperties.putBoolean(
                FirstRunActivity.SHOW_SEARCH_ENGINE_PAGE, shouldShowSearchEnginePage());
    }

    /** Marks a given flow as completed. */
    public static void markFlowAsCompleted() {
        // When the user accepts ToS in the Setup Wizard, we do not show the ToS page to the user
        // because the user has already accepted one outside FRE.
        if (!FirstRunUtils.isFirstRunEulaAccepted()) {
            FirstRunUtils.setEulaAccepted();
        }

        // Mark the FRE flow as complete.
        FirstRunStatus.setFirstRunFlowComplete(true);
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
        CrashKeys.getInstance().set(CrashKeyIndex.FIRST_RUN, "yes");

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

    /** Defines an alternative delegate for testing. Must be reset on {@code tearDown}. */
    @VisibleForTesting
    public static void setDelegateForTesting(FirstRunFlowSequencerDelegate delegate) {
        sDelegateForTesting = delegate;
    }
}
