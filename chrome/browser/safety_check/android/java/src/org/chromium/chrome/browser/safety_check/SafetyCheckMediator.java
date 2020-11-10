// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.BuildConfig;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_check.CompromisedCredential;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SecuritySettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.signin.SigninActivityLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

class SafetyCheckMediator implements PasswordCheck.Observer, SafetyCheckCommonObserver {
    /**
     * The minimal amount of time to show the checking state.
     * This is different from the standard guideline of 500ms because of the UX guidance for
     * Safety check on mobile and to be consistent with the Desktop counterpart (also 1s there).
     */
    private static final int CHECKING_MIN_DURATION_MS = 1000;
    /** Time after which the null-states will be shown: 10 minutes. */
    private static final long RESET_TO_NULL_AFTER_MS = 10 * DateUtils.MINUTE_IN_MILLIS;
    private static final String SAFETY_CHECK_INTERACTIONS = "Settings.SafetyCheck.Interactions";

    /** Bridge to the C++ side for the Safe Browsing and passwords checks. */
    private SafetyCheckBridge mSafetyCheckBridge;
    /** Model representing the current state of the checks. */
    private PropertyModel mModel;
    /** Client to interact with Omaha for the updates check. */
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    /** An instance of SettingsLauncher to start other activities. */
    private SettingsLauncher mSettingsLauncher;
    /** Client to launch a SigninActivity. */
    private SigninActivityLauncher mSigninLauncher;
    /** Async logic for password check. */
    private boolean mShowSafePasswordState;
    private boolean mPasswordsLoaded;
    private boolean mLeaksLoaded;

    // Indicates that the password check results are blocked on disk load at different stages.
    @IntDef({PasswordCheckLoadStage.IDLE, PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD,
            PasswordCheckLoadStage.COMPLETED_WAIT_FOR_LOAD})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PasswordCheckLoadStage {
        /** No need for action - nothing is blocked on data load. */
        int IDLE = 1;
        /** Apply the data from disk once available since this is initial load. */
        int INITIAL_WAIT_FOR_LOAD = 2;
        /** Apply the data from the latest run once available since this is a current check. */
        int COMPLETED_WAIT_FOR_LOAD = 3;
    }

    private @PasswordCheckLoadStage int mLoadStage;

    /** Callbacks and related objects to show the checking state for at least 1 second. */
    private Handler mHandler;
    private Runnable mRunnablePasswords;
    private Runnable mRunnableSafeBrowsing;
    private Runnable mRunnableUpdates;
    private long mCheckStartTime = -1;

    /**
     * UMA histogram values for Safety check interactions. Some value don't apply to Android.
     * Note: this should stay in sync with SettingsSafetyCheckInteractions in enums.xml.
     */
    @IntDef({SafetyCheckInteractions.STARTED, SafetyCheckInteractions.UPDATES_RELAUNCH,
            SafetyCheckInteractions.PASSWORDS_MANAGE, SafetyCheckInteractions.SAFE_BROWSING_MANAGE,
            SafetyCheckInteractions.EXTENSIONS_REVIEW,
            SafetyCheckInteractions.CHROME_CLEANER_REBOOT,
            SafetyCheckInteractions.CHROME_CLEANER_REVIEW,
            SafetyCheckInteractions.PASSWORDS_MANAGE_THROUGH_CARET_NAVIGATION,
            SafetyCheckInteractions.SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION,
            SafetyCheckInteractions.EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION,
            SafetyCheckInteractions.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SafetyCheckInteractions {
        int STARTED = 0;
        int UPDATES_RELAUNCH = 1;
        int PASSWORDS_MANAGE = 2;
        int SAFE_BROWSING_MANAGE = 3;
        int EXTENSIONS_REVIEW = 4;
        int CHROME_CLEANER_REBOOT = 5;
        int CHROME_CLEANER_REVIEW = 6;
        int PASSWORDS_MANAGE_THROUGH_CARET_NAVIGATION = 7;
        int SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION = 8;
        int EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION = 9;
        // New elements go above.
        int MAX_VALUE = EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION;
    }

    private final SharedPreferencesManager mPreferenceManager;

    /**
     * Callback that gets invoked once the result of the updates check is available. Not inlined
     * because a {@link WeakReference} needs to be passed (the check is asynchronous).
     */
    private final Callback<Integer> mUpdatesCheckCallback = (status) -> {
        mRunnableUpdates = () -> {
            if (mModel != null) {
                RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.UpdatesResult",
                        SafetyCheckProperties.updatesStateToNative(status),
                        UpdateStatus.MAX_VALUE + 1);
                mModel.set(SafetyCheckProperties.UPDATES_STATE, status);
            }
        };
        if (mHandler != null) {
            // Show the checking state for at least 1 second for a smoother UX.
            mHandler.postDelayed(mRunnableUpdates, getModelUpdateDelay());
        }
    };

    /**
     * Creates a new instance given a model, an updates client, and a settings launcher.
     *
     * @param model A model instance.
     * @param client An updates client.
     * @param settingsLauncher An instance of the {@link SettingsLauncher} implementation.
     * @param signinLauncher An instance implementing {@SigninActivityLauncher}.
     */
    public SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SigninActivityLauncher signinLauncher) {
        this(model, client, settingsLauncher, signinLauncher, null, new Handler());
        // Have to initialize this after the constructor call, since a "this" instance is needed.
        mSafetyCheckBridge = new SafetyCheckBridge(SafetyCheckMediator.this);
    }

    @VisibleForTesting
    SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SigninActivityLauncher signinLauncher,
            SafetyCheckBridge bridge, Handler handler) {
        mModel = model;
        mUpdatesClient = client;
        mSettingsLauncher = settingsLauncher;
        mSigninLauncher = signinLauncher;
        mSafetyCheckBridge = bridge;
        mHandler = handler;
        mPreferenceManager = SharedPreferencesManager.getInstance();
        // Set the listener for clicking the updates element.
        mModel.set(SafetyCheckProperties.UPDATES_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener) (p) -> {
                    if (!BuildConfig.IS_CHROME_BRANDED) {
                        return true;
                    }
                    String chromeAppId = ContextUtils.getApplicationContext().getPackageName();
                    // Open the Play Store page for the installed Chrome channel.
                    p.getContext().startActivity(new Intent(Intent.ACTION_VIEW,
                            Uri.parse(ContentUrlConstants.PLAY_STORE_URL_PREFIX + chromeAppId)));
                    return true;
                });
        // Set the listener for clicking the Safe Browsing element.
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener) (p) -> {
                    // Record UMA metrics.
                    RecordUserAction.record("Settings.SafetyCheck.ManageSafeBrowsing");
                    RecordHistogram.recordEnumeratedHistogram(SAFETY_CHECK_INTERACTIONS,
                            SafetyCheckInteractions.SAFE_BROWSING_MANAGE,
                            SafetyCheckInteractions.MAX_VALUE + 1);
                    String safeBrowsingSettingsClassName;
                    if (ChromeFeatureList.isEnabled(
                                ChromeFeatureList.SAFE_BROWSING_SECURITY_SECTION_UI)) {
                        // Open the Security settings since the flag for them is enabled.
                        safeBrowsingSettingsClassName = SecuritySettingsFragment.class.getName();
                        p.getContext().startActivity(settingsLauncher.createSettingsActivityIntent(
                                p.getContext(), safeBrowsingSettingsClassName,
                                SecuritySettingsFragment.createArguments(
                                        SettingsAccessPoint.SAFETY_CHECK)));
                    } else {
                        // Open the Sync and Services settings.
                        // TODO(crbug.com/1070620): replace the hardcoded class name with an import
                        // and ".class.getName()" once SyncAndServicesSettings is moved out of
                        // //chrome/android.
                        safeBrowsingSettingsClassName =
                                "org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings";
                        p.getContext().startActivity(settingsLauncher.createSettingsActivityIntent(
                                p.getContext(), safeBrowsingSettingsClassName));
                    }
                    return true;
                });
        // Set the listener for clicking the passwords element.
        updatePasswordElementClickDestination();
        // Set the listener for clicking the Check button.
        mModel.set(SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER,
                (View.OnClickListener) (v) -> performSafetyCheck());
        // Get the timestamp of the last run.
        mModel.set(SafetyCheckProperties.LAST_RUN_TIMESTAMP,
                mPreferenceManager.readLong(
                        ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0));
    }

    /**
     * Determines the initial state to show, triggering any fast checks if necessary based on the
     * last run timestamp.
     */
    public void setInitialState() {
        long currentTime = System.currentTimeMillis();
        long lastRun = mPreferenceManager.readLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0);
        if (currentTime - lastRun < RESET_TO_NULL_AFTER_MS) {
            mShowSafePasswordState = true;
            // Rerun the updates and Safe Browsing checks.
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            mSafetyCheckBridge.checkSafeBrowsing();
            mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
        } else {
            mShowSafePasswordState = false;
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.UNCHECKED);
        }
        mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.CHECKING);
        mLoadStage = PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD;
        // Reset the status of the password disk loads. If it's loaded, PasswordCheck will invoke
        // the callbacks again (the |callImmediatelyIfReady| argument to |addObserver| is true).
        mPasswordsLoaded = false;
        mLeaksLoaded = false;
        // If the user is not signed in, immediately set the state and do not block on disk loads.
        if (!mSafetyCheckBridge.userSignedIn()) {
            mLoadStage = PasswordCheckLoadStage.IDLE;
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SIGNED_OUT);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.SIGNED_OUT, PasswordsStatus.MAX_VALUE + 1);
            updatePasswordElementClickDestination();
        }
        PasswordCheckFactory.getOrCreate().addObserver(this, true);
        if (mPasswordsLoaded && mLeaksLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    /** Triggers all safety check child checks. */
    public void performSafetyCheck() {
        // Cancel pending delayed show callbacks if a new check is starting while any existing
        // elements are pending.
        cancelCallbacks();
        // Record the start action in UMA.
        RecordUserAction.record("Settings.SafetyCheck.Start");
        // Record the start interaction in the histogram.
        RecordHistogram.recordEnumeratedHistogram(SAFETY_CHECK_INTERACTIONS,
                SafetyCheckInteractions.STARTED, SafetyCheckInteractions.MAX_VALUE + 1);
        // Record the start time for tracking 1 second checking delay in the UI.
        mCheckStartTime = SystemClock.elapsedRealtime();
        // Record the absolute start time for showing when the last Safety check was performed.
        long currentTime = System.currentTimeMillis();
        mModel.set(SafetyCheckProperties.LAST_RUN_TIMESTAMP, currentTime);
        mPreferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, currentTime);
        // Increment the stored number of Safety check starts.
        mPreferenceManager.incrementInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER);
        // Set the checking state for all elements.
        mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.CHECKING);
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
        mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
        // Start all the checks.
        mSafetyCheckBridge.checkSafeBrowsing();
        // Start observing the password check events (including data loads).
        PasswordCheckFactory.getOrCreate().addObserver(this, false);
        // This indicates that the results of the initial data load should not be applied even if
        // they become available during the check.
        mLoadStage = PasswordCheckLoadStage.IDLE;
        PasswordCheckFactory.getOrCreate().startCheck();
        mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
    }

    /**
     * Gets invoked once the Safe Browsing check is completed.
     *
     * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state
     *     (see //components/safety_check/safety_check.h).
     */
    @Override
    public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {
        mRunnableSafeBrowsing = () -> {
            if (mModel != null) {
                RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.SafeBrowsingResult",
                        status, SafeBrowsingStatus.MAX_VALUE + 1);
                mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE,
                        SafetyCheckProperties.safeBrowsingStateFromNative(status));
            }
        };
        // Show the checking state for at least 1 second for a smoother UX.
        mHandler.postDelayed(mRunnableSafeBrowsing, getModelUpdateDelay());
    }

    /**
     * Gets invoked when the compromised credentials are fetched from the disk.
     * After this call, {@link PasswordCheck#getCompromisedCredentialsCount} returns a valid value.
     */
    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        mLeaksLoaded = true;
        if (mPasswordsLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    /**
     * Gets invoked when the saved passwords are fetched from the disk.
     * After this call, {@link PasswordCheck#getSavedPasswordsCount} returns a valid value.
     */
    @Override
    public void onSavedPasswordsFetchCompleted() {
        mPasswordsLoaded = true;
        if (mLeaksLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    /**
     * Gets invoked once the password check stops running.
     * @param status A {@link PasswordCheckUIStatus} enum value.
     */
    @Override
    public void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status) {
        if (status == PasswordCheckUIStatus.RUNNING || mLoadStage != PasswordCheckLoadStage.IDLE) {
            return;
        }
        // Handle error state.
        if (status != PasswordCheckUIStatus.IDLE) {
            mRunnablePasswords = () -> {
                if (mModel != null) {
                    @SafetyCheckProperties.PasswordsState
                    int state = SafetyCheckProperties.passwordsStatefromErrorState(status);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Settings.SafetyCheck.PasswordsResult",
                            SafetyCheckProperties.passwordsStateToNative(state),
                            PasswordsStatus.MAX_VALUE + 1);
                    mModel.set(SafetyCheckProperties.PASSWORDS_STATE, state);
                    updatePasswordElementClickDestination();
                }
            };
            // Show the checking state for at least 1 second for a smoother UX.
            mHandler.postDelayed(mRunnablePasswords, getModelUpdateDelay());
            return;
        }
        // Hand off the completed state to the method for handling loaded passwords data.
        mLoadStage = PasswordCheckLoadStage.COMPLETED_WAIT_FOR_LOAD;
        // If it's loaded already, should invoke the data handling method manually.
        if (mPasswordsLoaded && mLeaksLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    @Override
    public void onCompromisedCredentialFound(CompromisedCredential leakedCredential) {}

    @Override
    public void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {}

    /** Cancels any pending callbacks and registered observers.  */
    public void destroy() {
        cancelCallbacks();
        // Refresh the ref without creating a new one.
        PasswordCheck passwordCheck = PasswordCheckFactory.getPasswordCheckInstance();
        if (passwordCheck != null) {
            passwordCheck.stopCheck();
            passwordCheck.removeObserver(this);
        }
        mSafetyCheckBridge.destroy();
        mSafetyCheckBridge = null;
        mUpdatesClient = null;
        mModel = null;
        mHandler = null;
    }

    /** Cancels any delayed show callbacks. */
    private void cancelCallbacks() {
        if (mRunnablePasswords != null) {
            mHandler.removeCallbacks(mRunnablePasswords);
            mRunnablePasswords = null;
        }
        if (mRunnableSafeBrowsing != null) {
            mHandler.removeCallbacks(mRunnableSafeBrowsing);
            mRunnableSafeBrowsing = null;
        }
        if (mRunnableUpdates != null) {
            mHandler.removeCallbacks(mRunnableUpdates);
            mRunnableUpdates = null;
        }
    }

    /** Called when all data is loaded. Determines if it needs to update the model. */
    private void determinePasswordStateOnLoadComplete() {
        // Nothing is blocked on data load, so ignore the load.
        if (mLoadStage == PasswordCheckLoadStage.IDLE) return;
        // If something is blocked, that means the passwords check is being observed. At this point,
        // no further events need to be observed.
        PasswordCheckFactory.getOrCreate().removeObserver(this);
        // Only delay updating the UI on the user-triggered check and not initially.
        if (mLoadStage == PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD) {
            updatePasswordsStateOnDataLoaded();
        } else {
            // Show the checking state for at least 1 second for a smoother UX.
            mRunnablePasswords = this::updatePasswordsStateOnDataLoaded;
            mHandler.postDelayed(mRunnablePasswords, getModelUpdateDelay());
        }
    }

    /** Applies the results of the password check to the model. Only called when data is loaded. */
    private void updatePasswordsStateOnDataLoaded() {
        // Always display the compromised state.
        int compromised = PasswordCheckFactory.getOrCreate().getCompromisedCredentialsCount();
        if (compromised != 0) {
            mModel.set(SafetyCheckProperties.COMPROMISED_PASSWORDS, compromised);
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.COMPROMISED_EXIST);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.COMPROMISED_EXIST, PasswordsStatus.MAX_VALUE + 1);
        } else if (mLoadStage == PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD
                && !mShowSafePasswordState) {
            // Cannot show the safe state at the initial load if last run is older than 10 mins.
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.UNCHECKED);
        } else if (PasswordCheckFactory.getOrCreate().getSavedPasswordsCount() == 0) {
            // Can show safe state: display no passwords.
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.NO_PASSWORDS);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.NO_PASSWORDS, PasswordsStatus.MAX_VALUE + 1);
        } else {
            // Can show safe state: display no compromises.
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SAFE);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.SAFE, PasswordsStatus.MAX_VALUE + 1);
        }
        // Nothing is blocked on this any longer.
        mLoadStage = PasswordCheckLoadStage.IDLE;
        updatePasswordElementClickDestination();
    }

    /**
     * @return The delay in ms for updating the model in the running state.
     */
    private long getModelUpdateDelay() {
        return Math.max(
                0, mCheckStartTime + CHECKING_MIN_DURATION_MS - SystemClock.elapsedRealtime());
    }

    /** Sets the destination of the click on the passwords element based on the current state. */
    private void updatePasswordElementClickDestination() {
        @PasswordsState
        int state = mModel.get(SafetyCheckProperties.PASSWORDS_STATE);
        Preference.OnPreferenceClickListener listener = null;
        if (state == PasswordsState.SIGNED_OUT) {
            listener = (p) -> {
                // Open the sign in page.
                mSigninLauncher.launchActivity(p.getContext(), SigninAccessPoint.SAFETY_CHECK);
                return true;
            };
        } else if (state == PasswordsState.COMPROMISED_EXIST || state == PasswordsState.SAFE) {
            listener = (p) -> {
                // Record UMA metrics.
                RecordUserAction.record("Settings.SafetyCheck.ManagePasswords");
                RecordHistogram.recordEnumeratedHistogram(SAFETY_CHECK_INTERACTIONS,
                        SafetyCheckInteractions.PASSWORDS_MANAGE,
                        SafetyCheckInteractions.MAX_VALUE + 1);
                // Open the Password Check UI.
                PasswordCheckFactory.getOrCreate().showUi(
                        p.getContext(), PasswordCheckReferrer.SAFETY_CHECK);
                return true;
            };
        } else {
            listener = (p) -> {
                // Open the Passwords settings.
                PasswordManagerHelper.showPasswordSettings(
                        p.getContext(), ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncher);
                return true;
            };
        }
        mModel.set(SafetyCheckProperties.PASSWORDS_CLICK_LISTENER, listener);
    }
}
