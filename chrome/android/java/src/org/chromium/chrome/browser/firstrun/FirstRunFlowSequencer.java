// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.AuthTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * A helper to determine what should be the sequence of First Run Experience screens, and whether it
 * should be run.
 *
 * <p>Usage: new FirstRunFlowSequencer(activity, launcherProvidedProperties) { override
 * onFlowIsKnown }.start();
 */
@NullMarked
public abstract class FirstRunFlowSequencer {
    private static final String TAG = "firstrun";

    /**
     * A delegate class to determine if first run promo pages should be shown based on various
     * signals. Some methods may be overridden by tests to fake desired behavior.
     */
    @VisibleForTesting
    public static class FirstRunFlowSequencerDelegate {
        private final OneshotSupplier<ProfileProvider> mProfileSupplier;

        public FirstRunFlowSequencerDelegate(OneshotSupplier<ProfileProvider> profileSupplier) {
            mProfileSupplier = profileSupplier;
        }

        boolean shouldShowHistorySyncOptIn(boolean isChild) {
            assert mProfileSupplier.get() != null;
            Profile profile = mProfileSupplier.get().getOriginalProfile();
            HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
            if (isChild) {
                return !historySyncHelper.isHistorySyncDisabledByCustodian();
            }
            if (historySyncHelper.isHistorySyncDisabledByPolicy()
                    || historySyncHelper.didAlreadyOptIn()) {
                return false;
            }
            // Show the page only to signed-in users.
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(profile);
            assumeNonNull(identityManager);
            return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
        }

        /** @return true if the Search Engine promo page should be shown. */
        @VisibleForTesting
        public boolean shouldShowSearchEnginePage() {
            @SearchEnginePromoType
            int searchPromoType = LocaleManager.getInstance().getSearchEnginePromoShowType();
            return searchPromoType == SearchEnginePromoType.SHOW_NEW
                    || searchPromoType == SearchEnginePromoType.SHOW_EXISTING;
        }
    }

    /** Factory that provides Delegate instances for testing. */
    public interface DelegateFactoryForTesting {
        /** Build a test delegate for the given test. */
        FirstRunFlowSequencerDelegate buildFactory(
                OneshotSupplier<ProfileProvider> profileSupplier);
    }

    /**
     * The delegate to be used by the Sequencer. By default, it's an instance of
     * {@link FirstRunFlowSequencerDelegate}, unless it's overridden by {@code sDelegateForTesting}.
     */
    private final FirstRunFlowSequencerDelegate mDelegate;

    /** If not null, creates {@code mDelegate} for this object during tests. */
    private static @Nullable DelegateFactoryForTesting sDelegateFactoryForTesting;

    private boolean mIsFlowKnown;
    private boolean mAccountsAvailable;
    private @Nullable Boolean mIsChild;

    /**
     * Callback that is called once the flow is determined. If the properties is null, the First Run
     * experience needs to finish and restart the original intent if necessary.
     *
     * @param isChild A boolean value indicating child status.
     */
    public abstract void onFlowIsKnown(boolean isChild);

    public FirstRunFlowSequencer(
            OneshotSupplier<ProfileProvider> profileSupplier,
            OneshotSupplier<Boolean> childAccountStatusSupplier) {

        mDelegate =
                sDelegateFactoryForTesting != null
                        ? sDelegateFactoryForTesting.buildFactory(profileSupplier)
                        : new FirstRunFlowSequencerDelegate(profileSupplier);

        childAccountStatusSupplier.onAvailable(this::setChildAccountStatus);
    }

    /**
     * Starts determining parameters for the First Run. Once finished, calls onFlowIsKnown().
     *
     * <p>TODO(crbug.com/40223527): Add Supplier to AccountManagerFacadeProvider and remove this
     * method.
     */
    void start() {
        AccountManagerFacadeProvider.getInstance()
                .getAccounts()
                .then(
                        accounts -> {
                            RecordHistogram.recordCount1MHistogram(
                                    "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE",
                                    Math.min(accounts.size(), 2));

                            assert !mAccountsAvailable;
                            mAccountsAvailable = true;
                            maybeProcessFreEnvironmentPreNative();
                        });
    }

    @VisibleForTesting
    protected boolean shouldShowSearchEnginePage() {
        return mDelegate.shouldShowSearchEnginePage();
    }

    private boolean shouldShowHistorySyncOptIn() {
        return mDelegate.shouldShowHistorySyncOptIn(assumeNonNull(mIsChild));
    }

    private void setChildAccountStatus(boolean isChild) {
        assert mIsChild == null;
        mIsChild = isChild;
        maybeProcessFreEnvironmentPreNative();
    }

    private void maybeProcessFreEnvironmentPreNative() {
        // Wait till both child account status and the list of accounts are available.
        if (mIsChild == null || !mAccountsAvailable) return;

        if (mIsFlowKnown) return;
        mIsFlowKnown = true;
        onFlowIsKnown(mIsChild);
    }

    /**
     * Will be called when native is initialized and on-device policies are initialized (if any).
     *
     * @param freProperties Resulting FRE properties bundle.
     */
    public void updateFirstRunProperties(Bundle freProperties) {
        assert freProperties != null;
        freProperties.putBoolean(
                FirstRunActivity.SHOW_HISTORY_SYNC_PAGE, shouldShowHistorySyncOptIn());

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
        SigninPreferencesManager.getInstance()
                .setCctMismatchNoticeSuppressionPeriodStart(TimeUtils.currentTimeMillis());
    }

    /**
     * Checks if the First Run Experience needs to be launched.
     *
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @param fromIntent Intent used to launch the caller.
     * @return Whether the First Run Experience needs to be launched.
     */
    public static boolean checkIfFirstRunIsNecessary(
            boolean preferLightweightFre, Intent fromIntent) {
        boolean isCct =
                fromIntent.getBooleanExtra(
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
                || DeviceInfo.isRetailDemoMode()
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
     * @see {@link launch(Context, Intent, boolean, boolean)}
     */
    public static boolean launch(Context caller, Intent fromIntent) {
        boolean preferLightweightFre = false;
        if (!checkIfFirstRunIsNecessary(preferLightweightFre, fromIntent)) return false;

        boolean isAuthTab = AuthTabIntentDataProvider.isAuthTabIntent(fromIntent);
        boolean isCustomTab = LaunchIntentDispatcher.isCustomTabIntent(fromIntent);
        boolean cctFreInSameTask = ChromeFeatureList.sCctFreInSameTask.isEnabled();
        boolean inSameTask = isAuthTab || (cctFreInSameTask && isCustomTab);
        return launch(caller, fromIntent, preferLightweightFre, inSameTask);
    }

    /**
     * @see {@link launch(Context, Intent, boolean, boolean)}
     */
    public static boolean launch(Context caller, Intent fromIntent, boolean preferLightweightFre) {
        return launch(caller, fromIntent, preferLightweightFre, /* inSameTask= */ false);
    }

    /**
     * Tries to launch the First Run Experience. If the Activity was launched with the wrong Intent
     * flags, we first relaunch it to make sure it runs in its own task, then trigger First Run.
     *
     * @param caller Activity instance that is checking if first run is necessary.
     * @param fromIntent Intent used to launch the caller.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @param inSameTask Whether or not FRE will be launched in the same task as its caller.
     * @return Whether startup must be blocked (e.g. via Activity#finish or dropping the Intent).
     */
    public static boolean launch(
            Context caller, Intent fromIntent, boolean preferLightweightFre, boolean inSameTask) {
        // Check if the user needs to go through First Run at all.
        if (!checkIfFirstRunIsNecessary(preferLightweightFre, fromIntent)) return false;

        // Kickoff partner customization, since it's required for the first tab to load.
        PartnerBrowserCustomizations.getInstance().initializeAsync(caller.getApplicationContext());

        String intentUrl = IntentHandler.getUrlFromIntent(fromIntent);
        Uri uri = intentUrl != null ? Uri.parse(intentUrl) : null;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            caller.grantUriPermission(
                    caller.getPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        Log.d(TAG, "Redirecting user through FRE.");
        CrashKeys.getInstance().set(CrashKeyIndex.FIRST_RUN, "yes");

        if (inSameTask) {
            FreIntentCreator intentCreator = new FreIntentCreator();
            Intent freIntent =
                    intentCreator.create(
                            caller,
                            fromIntent,
                            preferLightweightFre,
                            /* usePendingIntent= */ false);
            freIntent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT);
            IntentUtils.safeStartActivity(caller, freIntent);
            return true;
        }

        if ((fromIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
            FreIntentCreator intentCreator = new FreIntentCreator();
            Intent freIntent =
                    intentCreator.create(
                            caller, fromIntent, preferLightweightFre, /* usePendingIntent= */ true);

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

    /** Allows specifying an alternative delegate for testing. */
    public static void setDelegateFactoryForTesting(DelegateFactoryForTesting factory) {
        sDelegateFactoryForTesting = factory;
        ResettersForTesting.register(() -> sDelegateFactoryForTesting = null);
    }
}
