// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.XPLAT_SYNCED_SETUP;
import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;
import static org.chromium.chrome.browser.sync.synced_set_up.SyncedSetUpUtilsBridge.getCrossDevicePrefsFromRemoteDevice;
import static org.chromium.chrome.browser.toolbar.settings.AddressBarPreference.computeToolbarPositionAndSource;
import static org.chromium.chrome.browser.toolbar.settings.AddressBarPreference.setToolbarPositionAndSource;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.TYPE_ACTION;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_IMPORT;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_REDO;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_UNDO;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.prefs.CrossDevicePrefTrackerFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker.CrossDevicePrefTrackerObserver;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.ServiceStatus;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.TimestampedPrefValue;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

@NullMarked
public class CrossDeviceSettingImporter implements TopResumedActivityChangedObserver {

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(CrossDeviceSettingImportOutcome)
    @IntDef({
        CrossDeviceSettingImportOutcome.SYNC_NOT_CONFIGURED,
        CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT,
        CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CrossDeviceSettingImportOutcome {
        int SYNC_NOT_CONFIGURED = 0;
        int NO_SETTINGS_TO_IMPORT = 1;
        int SNACKBAR_SHOWN = 2;
        int NUM_ENTRIES = 3;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:CrossDeviceSettingImportOutcome)

    private static final String TAG = "XplatSyncedSetup";

    // Fixed prefix used by CrossDevicePrefTracker for dictionary prefs with values from all devices
    private static final String CROSS_DEVICE_PREFIX = "cross_device.";

    // The ServiceStatuses where we need to wait for data to come in.
    private static final Set<Integer> NOT_READY_YET_STATES =
            Set.of(
                    ServiceStatus.DEVICE_INFO_TRACKER_MISSING,
                    ServiceStatus.LOCAL_DEVICE_INFO_MISSING,
                    ServiceStatus.SYNC_NOT_CONFIGURED_AND_LOCAL_DEVICE_INFO_MISSING,
                    ServiceStatus.WAITING_FOR_INITIAL_SYNC);

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final NullableObservableSupplier<Tab> mActivityTabSupplier;
    private final Context mContext;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onContentChanged(Tab tab) {
                    onTabChangeOrGainFocus(tab);
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    onTabChangeOrGainFocus(tab);
                }
            };

    private @Nullable Tab mObservedTab;
    private @Nullable CrossDevicePrefTracker mTrackerBeingObserved;
    private @Nullable CrossDevicePrefTrackerObserver mTrackerObserver;
    private @Nullable Runnable mLocalStateObserver;

    private final Callback<@Nullable Tab> mTabChangeCallback =
            new Callback<@Nullable Tab>() {
                @Override
                public void onResult(@Nullable Tab tab) {
                    if (mObservedTab != null) {
                        mObservedTab.removeObserver(mTabObserver);
                    }
                    mObservedTab = tab;
                    if (mObservedTab != null) {
                        mObservedTab.addObserver(mTabObserver);
                    }
                    onTabChangeOrGainFocus(tab);
                }
            };

    /**
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the current
     *     activity.
     * @param activityTabSupplier The supplier for the current activity's {@link Tab}.
     * @param context The current {@link Context}.
     * @param modalDialogManager The {@link ModalDialogManager} for the current activity.
     * @param snackbarManagerSupplier The supplier for the {@link SnackbarManager}.
     */
    public CrossDeviceSettingImporter(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            NullableObservableSupplier<Tab> activityTabSupplier,
            Context context,
            Supplier<@Nullable ModalDialogManager> modalDialogManager,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityTabSupplier = activityTabSupplier;
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManager;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mActivityLifecycleDispatcher.register(this);
        mActivityTabSupplier.addSyncObserverAndPostIfNonNull(mTabChangeCallback);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        if (!isTopResumedActivity) return;
        onTabChangeOrGainFocus(mActivityTabSupplier.get());
    }

    private void stopObservingTracker() {
        if (mTrackerObserver != null && mTrackerBeingObserved != null) {
            mTrackerBeingObserved.removeObserver(mTrackerObserver);
        }
        mTrackerObserver = null;
        mTrackerBeingObserved = null;
    }

    private void stopObservingLocalState() {
        if (mLocalStateObserver != null) {
            LocalStatePrefs.removeObserver(mLocalStateObserver);
        }
        mLocalStateObserver = null;
    }

    /**
     * Called when the current tab changes or gains focus.
     *
     * @param currentTab The current tab.
     */
    @VisibleForTesting
    void onTabChangeOrGainFocus(@Nullable Tab currentTab) {
        onTabChangeOrGainFocus(currentTab, /* availableImmediately= */ true);
    }

    private void onTabChangeOrGainFocus(@Nullable Tab currentTab, boolean availableImmediately) {
        if (!FeatureList.isNativeInitialized()
                || !ChromeFeatureList.isEnabled(XPLAT_SYNCED_SETUP)) {
            return;
        }

        if (currentTab == null) return;

        @Nullable Profile profile = currentTab.getProfile();
        if (profile == null) return;

        @Nullable CrossDevicePrefTracker crossDevicePrefTracker =
                CrossDevicePrefTrackerFactory.getForProfile(profile);
        if (crossDevicePrefTracker == null) return;

        @ServiceStatus int status = crossDevicePrefTracker.getServiceStatus();
        boolean trackerReady = !NOT_READY_YET_STATES.contains(status);
        boolean localStateReady = LocalStatePrefs.areNativePrefsLoaded();
        if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(
                    TAG,
                    "onTabChangeOrGainFocus - trackerReady = "
                            + trackerReady
                            + ", localStateReady = "
                            + localStateReady);
        }

        // If both dependencies are ready, stop any active observation and proceed to import.
        if (trackerReady && localStateReady) {
            stopObservingTracker();
            stopObservingLocalState();
            onCrossDevicePrefTrackerAndLocalStateReady(
                    crossDevicePrefTracker, status, profile, currentTab, availableImmediately);
            return;
        }

        // Otherwise, defer the logic by observing whichever dependency is not yet ready.
        if (!trackerReady) {
            ensureObservingTracker(crossDevicePrefTracker, profile);
        } else {
            stopObservingTracker();
        }

        if (!localStateReady) {
            ensureObservingLocalState();
        } else {
            stopObservingLocalState();
        }
    }

    private void ensureObservingTracker(CrossDevicePrefTracker tracker, Profile profile) {
        if (mTrackerBeingObserved != null && mTrackerBeingObserved != tracker) {
            stopObservingTracker();
        }
        if (mTrackerObserver != null) return;

        mTrackerObserver =
                new CrossDevicePrefTrackerObserver() {
                    @Override
                    public void onRemotePrefChanged(
                            String prefName,
                            TimestampedPrefValue timestampedPrefValue,
                            int osType,
                            int formFactor) {}

                    @Override
                    public void onServiceStatusChanged(int status) {
                        // If the tracker is still not ready, keep listening for status changes.
                        if (NOT_READY_YET_STATES.contains(status)) return;

                        // Ensure the tab and profile are still valid before retrying.
                        @Nullable Tab currentTab = mActivityTabSupplier.get();
                        if (currentTab == null) return;

                        @Nullable Profile currentProfile = currentTab.getProfile();
                        if (!profile.equals(currentProfile)) return;

                        onTabChangeOrGainFocus(currentTab, /* availableImmediately= */ false);
                    }
                };
        mTrackerBeingObserved = tracker;
        tracker.addObserver(mTrackerObserver);
    }

    private void ensureObservingLocalState() {
        if (mLocalStateObserver != null) return;

        if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(TAG, "Started observing local state");
        }
        mLocalStateObserver =
                () -> {
                    if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
                        Log.i(TAG, "Local state readiness observer was triggered");
                    }
                    onTabChangeOrGainFocus(
                            mActivityTabSupplier.get(), /* availableImmediately= */ false);
                };
        LocalStatePrefs.addObserver(mLocalStateObserver);
    }

    /**
     * Handles the {@link CrossDevicePrefTracker} and {@link LocalStatePrefs} reaching a "ready"
     * state.
     *
     * @param tracker The {@link CrossDevicePrefTracker}.
     * @param status The {@link ServiceStatus} of the tracker.
     * @param profile The {@link Profile}.
     * @param tab The {@link Tab} that is currently focused.
     * @param availableImmediately Whether the CrossDevicePrefTracker and LocalStatePrefs were
     *     available immediately (when we first checked).
     */
    @VisibleForTesting
    void onCrossDevicePrefTrackerAndLocalStateReady(
            CrossDevicePrefTracker tracker,
            @ServiceStatus int status,
            Profile profile,
            Tab tab,
            boolean availableImmediately) {
        if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(
                    TAG,
                    "running onCrossDevicePrefTrackerAndLocalStateReady with status "
                            + status
                            + ", available immediately ? "
                            + availableImmediately);
        }
        boolean onlyOmniboxPosition = !UrlUtilities.isNtpUrl(tab.getUrl());
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();
        if (onlyOmniboxPosition) {
            if (sharedPrefManager.readBoolean(
                    ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX,
                    /* defaultValue= */ true)) {
                return;
            }
        } else if (sharedPrefManager.readBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS,
                /* defaultValue= */ true)) {
            return;
        }

        // Record a single action for checking for remote settings, regardless of whether we're in
        // an omnibox-only case.
        recordAction(/* onlyOmniboxPosition= */ false, "CheckForRemoteSettings");
        if (status == ServiceStatus.AVAILABLE) {
            if (availableImmediately) {
                // If there was no delay, apply the settings immediately (skipping the user straight
                // to the undo prompt).
                applyAndNotifySettingImport(
                        profile,
                        getPrefsFromRemoteDevice(profile, tracker),
                        /* onlyOmniboxPosition= */ onlyOmniboxPosition);
            } else {
                // If there was a delay, ask the user whether they want to apply the settings.
                askToApplyNtpSettingImportIfNeeded(
                        profile,
                        getPrefsFromRemoteDevice(profile, tracker),
                        /* onlyOmniboxPosition= */ onlyOmniboxPosition);
            }
        } else {
            // If the status was not AVAILABLE, the user does not have their "Settings" sync toggle
            // on in their account settings.
            // Either way, because the CrossDevicePrefTracker became "ready", we are now done.
            markCrossDeviceSettingImportComplete(
                    onlyOmniboxPosition, CrossDeviceSettingImportOutcome.SYNC_NOT_CONFIGURED);
        }
    }

    /**
     * Marks (possibly only some of the) cross-device setting imports as complete.
     *
     * @param onlyOmniboxPosition Whether only the omnibox position setting is in scope.
     */
    private static void markCrossDeviceSettingImportComplete(
            boolean onlyOmniboxPosition, @CrossDeviceSettingImportOutcome int reason) {
        recordOutcome(reason);
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();

        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX, true);
        if (!onlyOmniboxPosition) {
            sharedPrefManager.writeBoolean(
                    ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, true);
        }
    }

    /**
     * Shows {@param snackbar} now if there no dialogs, or waits until the last dialog is dismissed
     * and then shows it.
     *
     * @param snackbar The {@link Snackbar} to show.
     * @param onlyOmniboxPosition Whether this snackbar only encompasses the bottom omnibox position
     *     pref.
     */
    @VisibleForTesting
    public void showSnackbarAfterDialogs(Snackbar snackbar, boolean onlyOmniboxPosition) {
        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        if (modalDialogManager == null) return;

        SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
        if (snackbarManager == null) return;

        if (modalDialogManager.isShowing()) {
            modalDialogManager.addObserver(
                    new ModalDialogManager.ModalDialogManagerObserver() {
                        @Override
                        public void onLastDialogDismissed() {
                            snackbarManager.showSnackbar(snackbar);
                            markCrossDeviceSettingImportComplete(
                                    onlyOmniboxPosition,
                                    CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
                        }
                    });
        } else {
            snackbarManager.showSnackbar(snackbar);
            markCrossDeviceSettingImportComplete(
                    onlyOmniboxPosition, CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to import NTP settings from another device.
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences that will be applied.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered. If true,
     *     we only check the omnibox position to determine whether to show the snackbar, and when we
     *     apply the new settings, only the omnibox position is applied. If false, all NTP settings
     *     AND the omnibox position are considered (both for determining whether to show the
     *     snackbar and applying the changes).
     */
    @VisibleForTesting
    void askToApplyNtpSettingImportIfNeeded(
            Profile profile, Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        if (shouldShowSnackbar(profile, preferencesToApply, onlyOmniboxPosition)) {
            Snackbar offerApplySnackbar =
                    Snackbar.make(
                            mContext.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    recordAction(onlyOmniboxPosition, "Apply");
                                    applyAndNotifySettingImport(
                                            profile, preferencesToApply, onlyOmniboxPosition);
                                }
                            },
                            TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_IMPORT);
            offerApplySnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.apply),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerApplySnackbar, onlyOmniboxPosition);
        } else {
            markCrossDeviceSettingImportComplete(
                    onlyOmniboxPosition, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
        }
    }

    /**
     * Applies settings from another device and shows a snackbar to the user, informing them that
     * their settings were applied and offering an undo button.
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences that will be applied.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     */
    private void applyAndNotifySettingImport(
            Profile profile, Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        if (shouldShowSnackbar(profile, preferencesToApply, onlyOmniboxPosition)) {
            Map<String, Object> currentPreferences = getCurrentSettings(profile);
            Snackbar offerUndoSnackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.synced_set_up_snackbar_applied_confirmation),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    if (onlyOmniboxPosition) {
                                        applyLocalStateSettings(currentPreferences);
                                    } else {
                                        applySettings(profile, currentPreferences);
                                    }

                                    recordAction(onlyOmniboxPosition, "Undo");
                                    askToRedoSettingImport(
                                            profile, preferencesToApply, onlyOmniboxPosition);
                                }
                            },
                            Snackbar.TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_UNDO);
            offerUndoSnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.undo),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerUndoSnackbar, onlyOmniboxPosition);
            applySettings(profile, preferencesToApply);
        } else {
            markCrossDeviceSettingImportComplete(
                    onlyOmniboxPosition, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to redo their NTP setting import (this is
     * offered after the user hits undo).
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences that will be applied during the redo.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     */
    private void askToRedoSettingImport(
            Profile profile, Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        Snackbar offerRedoSnackbar =
                Snackbar.make(
                        mContext.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                        new SnackbarManager.SnackbarController() {
                            @Override
                            public void onAction(@Nullable Object actionData) {
                                recordAction(onlyOmniboxPosition, "Redo");
                                applyAndNotifySettingImport(
                                        profile, preferencesToApply, onlyOmniboxPosition);
                            }
                        },
                        TYPE_ACTION,
                        UMA_CROSS_DEVICE_SETTING_REDO);
        offerRedoSnackbar.setAction(
                /* actionText= */ mContext.getString(R.string.redo), /* actionData= */ Map.of());
        showSnackbarAfterDialogs(offerRedoSnackbar, onlyOmniboxPosition);
    }

    /** Returns the user's current settings. */
    private Map<String, Object> getCurrentSettings(Profile profile) {
        Map<String, Object> result = new HashMap<>();

        PrefService localStatePrefs = LocalStatePrefs.get();
        if (localStatePrefs != null) {
            String omniboxPositionPref = Pref.IS_OMNIBOX_IN_BOTTOM_POSITION;
            result.put(omniboxPositionPref, localStatePrefs.getBoolean(omniboxPositionPref));
        }

        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs != null) {
            String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
            result.put(allCardsPref, userPrefs.getBoolean(allCardsPref));
            for (String key : MODULE_TYPE_TO_USER_PREFS_KEY.values()) {
                result.put(key, userPrefs.getBoolean(key));
            }
        }

        return result;
    }

    /**
     * @param profile The {@link Profile}.
     * @param preferences The preferences to check.
     * @return whether the user's current settings are different from {@param preferences}.
     */
    private boolean importedSettingsHavePreferenceChange(
            Profile profile, Map<String, Object> preferences) {
        if (!UserPrefs.areNativePrefsLoaded(profile)) return false;

        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs == null) {
            return false;
        }

        String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
        if (importedSettingHasPreferenceChange(preferences, userPrefs, allCardsPref)) {
            return true;
        }

        for (int moduleType : MODULE_TYPE_TO_USER_PREFS_KEY.keySet()) {
            @Nullable String key = MODULE_TYPE_TO_USER_PREFS_KEY.get(moduleType);
            if (key == null) continue;

            if (importedSettingHasPreferenceChange(preferences, userPrefs, key)) return true;
        }

        return importedSettingsHaveOmniboxChange(preferences);
    }

    /**
     * @param profile The {@link Profile}.
     * @param preferences The preferences to compare with local.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     * @return Whether the undo/redo snackbar should be shown.
     */
    private boolean shouldShowSnackbar(
            Profile profile, Map<String, Object> preferences, boolean onlyOmniboxPosition) {
        return onlyOmniboxPosition
                ? importedSettingsHaveOmniboxChange(preferences)
                : importedSettingsHavePreferenceChange(profile, preferences);
    }

    /**
     * @param preferences The preferences to check.
     * @return whether the user's current omnibox position is different from {@param preferences}.
     */
    private boolean importedSettingsHaveOmniboxChange(Map<String, Object> preferences) {
        PrefService localPrefs = LocalStatePrefs.get();
        if (localPrefs == null) {
            return false;
        }

        @Nullable Object bottomOmniboxValue = preferences.get(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION);
        if (bottomOmniboxValue != null
                && bottomOmniboxValue instanceof Boolean bottomOmniboxBoolean) {
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
                Log.i(
                        TAG,
                        "importedSettingsHaveOmniboxChange, bottomOmniboxBoolean = "
                                + bottomOmniboxBoolean
                                + ", localPrefs.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION) = "
                                + localPrefs.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION));
            }
            return bottomOmniboxBoolean
                    != localPrefs.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION);
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(TAG, "importedSettingsHaveOmniboxChange, returning false at bottom of function");
        }
        return false;
    }

    /**
     * @param preferences The preferences to check.
     * @param userPrefs The user's current preferences.
     * @param key The key of the preference to check.
     * @return whether the user's current settings are different from {@param preferences} for the
     *     given {@param key}.
     */
    private boolean importedSettingHasPreferenceChange(
            Map<String, Object> preferences, PrefService userPrefs, String key) {
        @Nullable Object preferencesValue = preferences.get(key);
        // If the key is not in 'preferences' and userPrefs is using a non-default value
        return (preferencesValue == null && !userPrefs.isDefaultValuePreference(key))
                ||
                // or key is in 'preferences' and userPrefs has a different value
                (preferencesValue instanceof Boolean booleanPrefValue
                        && booleanPrefValue != userPrefs.getBoolean(key));
    }

    /**
     * Applies the given {@param preferencesToApply}.
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences to apply.
     */
    private void applySettings(Profile profile, Map<String, Object> preferencesToApply) {
        applyUserPrefSettings(profile, preferencesToApply);
        applyLocalStateSettings(preferencesToApply);
    }

    /**
     * Applies the user pref settings from {@param preferencesToApply}.
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences to apply.
     */
    private void applyUserPrefSettings(Profile profile, Map<String, Object> preferencesToApply) {
        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs == null) return;

        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();

        String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
        @Nullable Object allCardsPrefValue = preferencesToApply.get(allCardsPref);
        if (allCardsPrefValue instanceof Boolean allCardsPrefBoolean) {
            homeModulesConfigManager.setPrefAllCardsEnabled(allCardsPrefBoolean);
        }

        for (int moduleType : MODULE_TYPE_TO_USER_PREFS_KEY.keySet()) {
            String userPrefKey = MODULE_TYPE_TO_USER_PREFS_KEY.get(moduleType);
            if (userPrefKey == null) continue;

            Object value = preferencesToApply.get(userPrefKey);
            if (value == null) {
                // Invalid key.
                continue;
            }

            if (value instanceof Boolean booleanValue) {
                userPrefs.setBoolean(userPrefKey, booleanValue);
                homeModulesConfigManager.setPrefModuleTypeEnabled(moduleType, booleanValue);
            }
        }
    }

    /**
     * Applies the local state settings from {@param preferencesToApply}.
     *
     * <p>NOTE: currently, the ONLY local state setting is the omnibox position setting. Refactoring
     * will be required if more local state settings are added in the future.
     *
     * @param preferencesToApply The preferences to apply.
     */
    private void applyLocalStateSettings(Map<String, Object> preferencesToApply) {
        PrefService localStatePrefs = LocalStatePrefs.get();
        if (localStatePrefs == null) return;

        String omniboxKey = Pref.IS_OMNIBOX_IN_BOTTOM_POSITION;
        if (!preferencesToApply.containsKey(omniboxKey)) return;

        if (preferencesToApply.get(omniboxKey) instanceof Boolean booleanValue) {
            localStatePrefs.setBoolean(omniboxKey, booleanValue);
        }

        // Force an update from LocalStatePrefs to AddressBarPreference.
        setToolbarPositionAndSource(computeToolbarPositionAndSource());
    }

    /**
     * Get a map of prefs to values, stripped of the "cross_device." prefix.
     *
     * @param profile The {@link Profile}.
     * @param tracker The {@link CrossDevicePrefTracker}.
     * @return The map of prefs to values.
     */
    @VisibleForTesting
    Map<String, Object> getPrefsFromRemoteDevice(Profile profile, CrossDevicePrefTracker tracker) {
        Map<String, Object> crossDevicePrefs =
                getCrossDevicePrefsFromRemoteDevice(tracker, profile);
        Map<String, Object> res = new HashMap<>();
        for (String crossDeviceKey : crossDevicePrefs.keySet()) {
            String key =
                    crossDeviceKey.replaceAll(
                            /* regex= */ "^" + CROSS_DEVICE_PREFIX, /* replacement= */ "");
            res.put(key, crossDevicePrefs.get(crossDeviceKey));
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(TAG, "getPrefsFromRemoteDevice, res = " + res);
        }
        return res;
    }

    /** Logs UMA with suffix {@param suffix} (omnibox-specific if {@param onlyOmniboxPosition}) */
    private void recordAction(boolean onlyOmniboxPosition, String suffix) {
        StringBuilder action = new StringBuilder("Android.CrossDeviceSettingImport");
        if (onlyOmniboxPosition) {
            action.append(".OmniboxPosition");
        }
        action.append('.');
        action.append(suffix);
        RecordUserAction.record(action.toString());
    }

    /** Logs outcome of cross device setting import (reports showing the feature, or why not. */
    private static void recordOutcome(@CrossDeviceSettingImportOutcome int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sync.CrossDeviceSettingImportOutcome",
                value,
                CrossDeviceSettingImportOutcome.NUM_ENTRIES);
    }

    /** Destroys the {@link CrossDeviceSettingImporter}. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityTabSupplier.removeObserver(mTabChangeCallback);
        if (mObservedTab != null) {
            mObservedTab.removeObserver(mTabObserver);
            mObservedTab = null;
        }
        stopObservingTracker();
        stopObservingLocalState();
    }
}
