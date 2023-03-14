// Copyright 2020 The Chromium Authors
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

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.Optional;

class SafetyCheckMediator
        implements PasswordCheck.Observer, PasswordStoreBridge.PasswordStoreObserver {
    /**
     * The minimal amount of time to show the checking state.
     * This needs to be non-zero to make it seem like the browser is doing work. This is different
     * from the standard guideline of 500ms because of the UX guidance for Safety check on mobile
     * and to be consistent with the Desktop counterpart (also 1s there).
     */
    private static final int CHECKING_MIN_DURATION_MS = 1000;
    /** Time after which the null-states will be shown: 10 minutes. */
    private static final long RESET_TO_NULL_AFTER_MS = 10 * DateUtils.MINUTE_IN_MILLIS;
    private static final String SAFETY_CHECK_INTERACTIONS = "Settings.SafetyCheck.Interactions";

    /** Model representing the current state of the checks. */
    private PropertyModel mModel;
    /** Client to interact with Omaha for the updates check. */
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    /** An instance of SettingsLauncher to start other activities. */
    private SettingsLauncher mSettingsLauncher;
    /** Client to launch a SigninActivity. */
    private SyncConsentActivityLauncher mSigninLauncher;
    /** Async logic for password check. */
    private boolean mShowSafePasswordState;
    /** Password store bridge. TODO(crbug.com/1315267): Move this into a new class. */
    private PasswordStoreBridge mPasswordStoreBridge;
    private boolean mPasswordsLoaded;
    private boolean mLeaksLoaded;

    private ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

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
    private Integer mBreachedCredentialsCount = 0;

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
        if (mHandler == null) return;

        setRunnableUpdates(() -> {
            if (mModel != null) {
                RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.UpdatesResult",
                        SafetyCheckProperties.updatesStateToNative(status),
                        UpdateStatus.MAX_VALUE + 1);
                mModel.set(SafetyCheckProperties.UPDATES_STATE, status);
            }
        });
    };

    /**
     * Creates a new instance given a model, an updates client, and a settings launcher.
     *
     * @param model A model instance.
     * @param client An updates client.
     * @param settingsLauncher An instance of the {@link SettingsLauncher} implementation.
     * @param signinLauncher An instance implementing {@SigninActivityLauncher}.
     * @param modalDialogManagerSupplier A supplier for the {@link ModalDialogManager}.
     */
    public SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SyncConsentActivityLauncher signinLauncher,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        this(model, client, settingsLauncher, signinLauncher, new Handler());
        mPasswordStoreBridge = new PasswordStoreBridge();
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @VisibleForTesting
    SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SyncConsentActivityLauncher signinLauncher,
            PasswordStoreBridge bridge, Handler handler) {
        this(model, client, settingsLauncher, signinLauncher, handler);
        mPasswordStoreBridge = bridge;
    }

    SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SyncConsentActivityLauncher signinLauncher,
            Handler handler) {
        mModel = model;
        mUpdatesClient = client;
        mSettingsLauncher = settingsLauncher;
        mSigninLauncher = signinLauncher;
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
                    // Open the Safe Browsing settings.
                    safeBrowsingSettingsClassName = SafeBrowsingSettingsFragment.class.getName();
                    p.getContext().startActivity(settingsLauncher.createSettingsActivityIntent(
                            p.getContext(), safeBrowsingSettingsClassName,
                            SafeBrowsingSettingsFragment.createArguments(
                                    SettingsAccessPoint.SAFETY_CHECK)));
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
            checkSafeBrowsing();
            mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
        } else {
            mShowSafePasswordState = false;
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.UNCHECKED);

            // If the new Password Manager backend is out of date, attempting to fetch breached
            // credentials will expectedly fail and display an error message. This error is
            // designed to be only shown when user explicitly runs the check (or it was ran
            // recently). For this case, breached credential fetch is skipped.
            if (PasswordManagerHelper.canUseUpm()
                    && PasswordManagerBackendSupportHelper.getInstance().isUpdateNeeded()) {
                mLoadStage = PasswordCheckLoadStage.IDLE;
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.UNCHECKED);
                return;
            }
        }
        mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.CHECKING);
        mLoadStage = PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD;
        // If the user is not signed in, immediately set the state and do not block on disk loads.
        if (!SafetyCheckBridge.userSignedIn()) {
            mLoadStage = PasswordCheckLoadStage.IDLE;
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SIGNED_OUT);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.SIGNED_OUT, PasswordsStatus.MAX_VALUE + 1);
            updatePasswordElementClickDestination();
        }

        fetchPasswordsAndBreachedCredentials();
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
        checkSafeBrowsing();
        checkPasswords();
        mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
    }

    /**
     * Gets invoked when the compromised credentials are fetched from the disk.
     * After this call, {@link PasswordCheck#getCompromisedCredentialsCount} returns a valid value.
     */
    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        mLeaksLoaded = true;
        mBreachedCredentialsCount = PasswordCheckFactory.getOrCreate(mSettingsLauncher)
                                            .getCompromisedCredentialsCount();
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
        onPasswordsLoaded();
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

        if (mModel == null) return;

        // Handle error state.
        if (status != PasswordCheckUIStatus.IDLE) {
            setRunnablePasswords(() -> {
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
            });
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
    public void onPasswordCheckProgressChanged(int alreadyProcessed, int remainingInQueue) {}

    /**
     *  PasswordStoreBridge.PasswordStoreObserver implementation.
     */

    /**
     * Gets invoked when the passwords are fetched from the disk or have changed.
     * After this call, {@link PasswordStoreBridge#getPasswordStoreCredentialsCount} returns a valid
     * value.
     */
    @Override
    public void onSavedPasswordsChanged(int count) {
        onPasswordsLoaded();
    }

    /**
     * Not used by mediator as Password edit event isn't interesting.
     */
    @Override
    public void onEdit(PasswordStoreCredential credential) {}

    /** Cancels any pending callbacks and registered observers.  */
    public void destroy() {
        cancelCallbacks();
        if (!PasswordManagerHelper.canUseUpm()) {
            // Refresh the ref without creating a new one.
            PasswordCheck passwordCheck = PasswordCheckFactory.getPasswordCheckInstance();
            if (passwordCheck != null) {
                passwordCheck.stopCheck();
                passwordCheck.removeObserver(this);
            }
        } else {
            mPasswordStoreBridge.removeObserver(this);
            mPasswordStoreBridge.destroy();
        }
        mUpdatesClient = null;
        mModel = null;
        mHandler = null;
    }

    /** Cancels any delayed show callbacks. */
    private void cancelCallbacks() {
        setRunnablePasswords(null);
        setRunnableSafeBrowsing(null);
        setRunnableUpdates(null);
    }

    /**
     * Sets {@link mRunnablePasswords} and, if non-null, runs it with a delay.
     * Will cancel any outstanding callbacks set by previous calls to this method.
     */
    private void setRunnablePasswords(Runnable r) {
        if (mRunnablePasswords != null) {
            mHandler.removeCallbacks(mRunnablePasswords);
        }
        mRunnablePasswords = r;
        if (mRunnablePasswords != null) {
            mHandler.postDelayed(mRunnablePasswords, getModelUpdateDelay());
        }
    }

    /**
     * Sets {@link mRunnableSafeBrowsing} and, if non-null, runs it with a delay.
     * Will cancel any outstanding callbacks set by previous calls to this method.
     */
    private void setRunnableSafeBrowsing(Runnable r) {
        if (mRunnableSafeBrowsing != null) {
            mHandler.removeCallbacks(mRunnableSafeBrowsing);
        }
        mRunnableSafeBrowsing = r;
        if (mRunnableSafeBrowsing != null) {
            mHandler.postDelayed(mRunnableSafeBrowsing, getModelUpdateDelay());
        }
    }

    /**
     * Sets {@link mRunnableUpdates} and, if non-null, runs it with a delay.
     * Will cancel any outstanding callbacks set by previous calls to this method.
     */
    private void setRunnableUpdates(Runnable r) {
        if (mRunnableUpdates != null) {
            mHandler.removeCallbacks(mRunnableUpdates);
        }
        mRunnableUpdates = r;
        if (mRunnableUpdates != null) {
            mHandler.postDelayed(mRunnableUpdates, getModelUpdateDelay());
        }
    }

    private void checkSafeBrowsing() {
        setRunnableSafeBrowsing(() -> {
            if (mModel != null) {
                @SafeBrowsingStatus
                int status = SafetyCheckBridge.checkSafeBrowsing();
                RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.SafeBrowsingResult",
                        status, SafeBrowsingStatus.MAX_VALUE + 1);
                mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE,
                        SafetyCheckProperties.safeBrowsingStateFromNative(status));
            }
        });
    }

    /** Called when all data is loaded. Determines if it needs to update the model. */
    private void determinePasswordStateOnLoadComplete() {
        // Nothing is blocked on data load, so ignore the load.
        if (mLoadStage == PasswordCheckLoadStage.IDLE) return;
        if (!PasswordManagerHelper.canUseUpm()) {
            // If something is blocked, that means the passwords check is being observed. At this
            // point, no further events need to be observed.
            PasswordCheckFactory.getOrCreate(mSettingsLauncher).removeObserver(this);
        } else {
            mPasswordStoreBridge.removeObserver(this);
        }
        // Only delay updating the UI on the user-triggered check and not initially.
        if (mLoadStage == PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD) {
            updatePasswordsStateOnDataLoaded();
        } else {
            setRunnablePasswords(this::updatePasswordsStateOnDataLoaded);
        }
    }

    /** Applies the results of the password check to the model. Only called when data is loaded. */
    private void updatePasswordsStateOnDataLoaded() {
        // Always display the compromised state.
        if (mBreachedCredentialsCount != 0) {
            mModel.set(SafetyCheckProperties.COMPROMISED_PASSWORDS, mBreachedCredentialsCount);
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.COMPROMISED_EXIST);
            // Record the value in UMA.
            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    PasswordsStatus.COMPROMISED_EXIST, PasswordsStatus.MAX_VALUE + 1);
        } else if (mLoadStage == PasswordCheckLoadStage.INITIAL_WAIT_FOR_LOAD
                && !mShowSafePasswordState) {
            // Cannot show the safe state at the initial load if last run is older than 10 mins.
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.UNCHECKED);
        } else if (!hasSavedPasswords()) {
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
                mSigninLauncher.launchActivityIfAllowed(
                        p.getContext(), SigninAccessPoint.SAFETY_CHECK);
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
                if (!PasswordManagerHelper.canUseUpm()) {
                    PasswordCheckFactory.getOrCreate(mSettingsLauncher)
                            .showUi(p.getContext(), PasswordCheckReferrer.SAFETY_CHECK);
                } else {
                    PasswordManagerHelper.showPasswordCheckup(p.getContext(),
                            PasswordCheckReferrer.SAFETY_CHECK, SyncService.get(),
                            mModalDialogManagerSupplier);
                }
                return true;
            };
        } else if (state == PasswordsState.BACKEND_VERSION_NOT_SUPPORTED) {
            listener = (p) -> {
                PasswordManagerHelper.launchGmsUpdate(p.getContext());
                return true;
            };
        } else {
            listener = (p) -> {
                PasswordManagerHelper.showPasswordSettings(p.getContext(),
                        ManagePasswordsReferrer.SAFETY_CHECK, mSettingsLauncher, SyncService.get(),
                        mModalDialogManagerSupplier, /*managePasskeys=*/false);
                return true;
            };
        }
        mModel.set(SafetyCheckProperties.PASSWORDS_CLICK_LISTENER, listener);
    }

    private void onPasswordsLoaded() {
        if (mModel == null) return;

        mPasswordsLoaded = true;
        if (mLeaksLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    private void fetchPasswordsAndBreachedCredentials() {
        mLeaksLoaded = false;
        mPasswordsLoaded = false;
        if (!PasswordManagerHelper.canUseUpm()) {
            // Reset the status of the password disk loads. If it's loaded, PasswordCheck will
            // invoke the callbacks again (the |callImmediatelyIfReady| argument to |addObserver| is
            // true).
            PasswordCheckFactory.getOrCreate(mSettingsLauncher).addObserver(this, true);
            return;
        }

        mPasswordStoreBridge.addObserver(this, true);
        WeakReference<SafetyCheckMediator> weakRef = new WeakReference(this);
        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                getSyncingAccount(),
                count
                -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onBreachedCredentialsObtained(count, false);
                },
                error -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onPasswordCheckFailed(error);
                });
    }

    private void checkPasswords() {
        mLoadStage = PasswordCheckLoadStage.IDLE;

        if (!PasswordManagerHelper.canUseUpm()) {
            // Start observing the password check events (including data loads).
            PasswordCheckFactory.getOrCreate(mSettingsLauncher).addObserver(this, false);
            // This indicates that the results of the initial data load should not be applied even
            // if they become available during the check.
            PasswordCheckFactory.getOrCreate(mSettingsLauncher).startCheck();
            return;
        }

        WeakReference<SafetyCheckMediator> weakRef = new WeakReference(this);
        PasswordManagerHelper.runPasswordCheckupInBackground(PasswordCheckReferrer.SAFETY_CHECK,
                getSyncingAccount(),
                unused
                -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onPasswordCheckFinished();
                },
                error -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onPasswordCheckFailed(error);
                });
    }

    private boolean hasSavedPasswords() {
        if (!PasswordManagerHelper.canUseUpm()) {
            return PasswordCheckFactory.getOrCreate(mSettingsLauncher).getSavedPasswordsCount() > 0;
        }
        return mPasswordStoreBridge.getPasswordStoreCredentialsCount() > 0;
    }

    /**
     * Following methods are used only with the new PasswordCheck API.
     */

    private void onBreachedCredentialsObtained(Integer count, boolean duringCheck) {
        if (mModel == null) return;

        if (duringCheck) {
            // Hand off the completed state to the method for handling loaded passwords data.
            mLoadStage = PasswordCheckLoadStage.COMPLETED_WAIT_FOR_LOAD;
        }

        mBreachedCredentialsCount = count;
        mLeaksLoaded = true;
        if (mPasswordsLoaded) {
            determinePasswordStateOnLoadComplete();
        }
    }

    private void onPasswordCheckFinished() {
        if (mModel == null) return;

        WeakReference<SafetyCheckMediator> weakRef = new WeakReference(this);
        PasswordManagerHelper.getBreachedCredentialsCount(PasswordCheckReferrer.SAFETY_CHECK,
                getSyncingAccount(),
                count
                -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onBreachedCredentialsObtained(count, true);
                },
                error -> {
                    SafetyCheckMediator mediator = weakRef.get();
                    if (mediator == null) return;
                    mediator.onPasswordCheckFailed(error);
                });
    }

    private void onPasswordCheckFailed(Exception error) {
        if (mModel == null) return;

        setRunnablePasswords(() -> {
            if (mModel == null) return;

            RecordHistogram.recordEnumeratedHistogram("Settings.SafetyCheck.PasswordsResult",
                    SafetyCheckProperties.passwordsStateToNative(PasswordsState.ERROR),
                    PasswordsStatus.MAX_VALUE + 1);
            if (error instanceof PasswordCheckBackendException
                    && ((PasswordCheckBackendException) error).errorCode
                            == CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE,
                        PasswordsState.BACKEND_VERSION_NOT_SUPPORTED);
            } else {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.ERROR);
            }

            updatePasswordElementClickDestination();
        });
    }

    private Optional<String> getSyncingAccount() {
        return PasswordManagerHelper.hasChosenToSyncPasswords(SyncService.get())
                ? Optional.of(CoreAccountInfo.getEmailFrom(SyncService.get().getAccountInfo()))
                : Optional.empty();
    }
}
