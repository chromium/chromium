// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.chromium.build.NullUtil.assumeNonNull;
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

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
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

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;

@NullMarked
public class CrossDeviceSettingImporter implements TopResumedActivityChangedObserver {

    private static final String TAG = "XplatSyncedSetup";

    // Fixed prefix used by CrossDevicePrefTracker for dictionary prefs with values from all devices
    private static final String CROSS_DEVICE_PREFIX = "cross_device.";

    // The ServiceStatuses where we need to wait for data to come in.
    private static final Set<Integer> NOT_READY_YET_STATES =
            Set.of(
                    ServiceStatus.DEVICE_INFO_TRACKER_MISSING,
                    ServiceStatus.LOCAL_DEVICE_INFO_MISSING);

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final NullableObservableSupplier<Tab> mActivityTabSupplier;
    private final Context mContext;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
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
            Supplier<ModalDialogManager> modalDialogManager,
            Supplier<SnackbarManager> snackbarManagerSupplier) {
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

    /**
     * Called when the current tab changes or gains focus.
     *
     * @param currentTab The current tab.
     */
    @VisibleForTesting
    void onTabChangeOrGainFocus(@Nullable Tab currentTab) {
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
        if (NOT_READY_YET_STATES.contains(status)) {
            crossDevicePrefTracker.addObserver(
                    new CrossDevicePrefTrackerObserver() {
                        @Override
                        public void onRemotePrefChanged(
                                String prefName,
                                TimestampedPrefValue timestampedPrefValue,
                                int osType,
                                int formFactor) {}

                        @Override
                        public void onServiceStatusChanged(int status) {
                            onCrossDevicePrefTrackerReady(
                                    crossDevicePrefTracker,
                                    status,
                                    profile,
                                    currentTab,
                                    /* availableImmediately= */ false);
                        }
                    });
        } else {
            onCrossDevicePrefTrackerReady(
                    crossDevicePrefTracker,
                    status,
                    profile,
                    currentTab,
                    /* availableImmediately= */ true);
        }
    }

    /**
     * Handles the {@link CrossDevicePrefTracker} reaching a "ready" state.
     *
     * @param tracker The {@link CrossDevicePrefTracker}.
     * @param status The {@link ServiceStatus} of the tracker.
     * @param profile The {@link Profile}.
     * @param tab The {@link Tab} that is currently focused.
     * @param availableImmediately Whether the CrossDevicePrefTracker was available immediately
     *     (when we first checked).
     */
    @VisibleForTesting
    void onCrossDevicePrefTrackerReady(
            CrossDevicePrefTracker tracker,
            @ServiceStatus int status,
            Profile profile,
            Tab tab,
            boolean availableImmediately) {
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

        if (status == ServiceStatus.AVAILABLE) {
            if (availableImmediately) {
                // If there was no delay, apply the settings immediately (skipping the user straight
                // to the undo prompt).
                applyAndNotifySettingImport(
                        getPrefsFromRemoteDevice(tracker, profile),
                        /* onlyOmniboxPosition= */ onlyOmniboxPosition);
            } else {
                // If there was a delay, ask the user whether they want to apply the settings.
                askToApplyNtpSettingImportIfNeeded(
                        getPrefsFromRemoteDevice(tracker, profile),
                        /* onlyOmniboxPosition= */ onlyOmniboxPosition);
            }
        } else {
            // If the status was not AVAILABLE, the user does not have their "Settings" sync toggle
            // on in their account settings.
            // Either way, because the CrossDevicePrefTracker became "ready", we are now done.
            markCrossDeviceSettingImportComplete(onlyOmniboxPosition);
        }
    }

    /**
     * Marks (possibly only some of the) cross-device setting imports as complete.
     *
     * @param onlyOmniboxPosition Whether only the omnibox position setting is in scope.
     */
    private static void markCrossDeviceSettingImportComplete(boolean onlyOmniboxPosition) {
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
                            markCrossDeviceSettingImportComplete(onlyOmniboxPosition);
                        }
                    });
        } else {
            snackbarManager.showSnackbar(snackbar);
            markCrossDeviceSettingImportComplete(onlyOmniboxPosition);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to import NTP settings from another device.
     *
     * @param preferencesToApply The preferences that will be applied.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered. If true,
     *     we only check the omnibox position to determine whether to show the snackbar, and when we
     *     apply the new settings, only the omnibox position is applied. If false, all NTP settings
     *     AND the omnibox position are considered (both for determining whether to show the
     *     snackbar and applying the changes).
     */
    @VisibleForTesting
    void askToApplyNtpSettingImportIfNeeded(
            Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        if (shouldShowSnackbar(preferencesToApply, onlyOmniboxPosition)) {
            Snackbar offerApplySnackbar =
                    Snackbar.make(
                            mContext.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    recordUma(onlyOmniboxPosition, "Apply");
                                    applyAndNotifySettingImport(
                                            preferencesToApply, onlyOmniboxPosition);
                                }
                            },
                            TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_IMPORT);
            offerApplySnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.apply),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerApplySnackbar, onlyOmniboxPosition);
        } else {
            markCrossDeviceSettingImportComplete(onlyOmniboxPosition);
        }
    }

    /**
     * Applies settings from another device and shows a snackbar to the user, informing them that
     * their settings were applied and offering an undo button.
     *
     * @param preferencesToApply The preferences that will be applied.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     */
    private void applyAndNotifySettingImport(
            Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        if (shouldShowSnackbar(preferencesToApply, onlyOmniboxPosition)) {
            Map<String, Object> currentPreferences = getCurrentSettings();
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
                                        applySettings(currentPreferences);
                                    }

                                    recordUma(onlyOmniboxPosition, "Undo");
                                    askToRedoSettingImport(preferencesToApply, onlyOmniboxPosition);
                                }
                            },
                            Snackbar.TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_UNDO);
            offerUndoSnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.undo),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerUndoSnackbar, onlyOmniboxPosition);
            applySettings(preferencesToApply);
        } else {
            markCrossDeviceSettingImportComplete(onlyOmniboxPosition);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to redo their NTP setting import (this is
     * offered after the user hits undo).
     *
     * @param preferencesToApply The preferences that will be applied during the redo.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     */
    private void askToRedoSettingImport(
            Map<String, Object> preferencesToApply, boolean onlyOmniboxPosition) {
        Snackbar offerRedoSnackbar =
                Snackbar.make(
                        mContext.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                        new SnackbarManager.SnackbarController() {
                            @Override
                            public void onAction(@Nullable Object actionData) {
                                recordUma(onlyOmniboxPosition, "Redo");
                                applyAndNotifySettingImport(
                                        preferencesToApply, onlyOmniboxPosition);
                            }
                        },
                        TYPE_ACTION,
                        UMA_CROSS_DEVICE_SETTING_REDO);
        offerRedoSnackbar.setAction(
                /* actionText= */ mContext.getString(R.string.redo), /* actionData= */ Map.of());
        showSnackbarAfterDialogs(offerRedoSnackbar, onlyOmniboxPosition);
    }

    /** Returns the user's current settings. */
    private Map<String, Object> getCurrentSettings() {
        Map<String, Object> result = new HashMap<>();

        PrefService localStatePrefs = LocalStatePrefs.get();
        if (localStatePrefs != null) {
            String omniboxPositionPref = Pref.IS_OMNIBOX_IN_BOTTOM_POSITION;
            result.put(omniboxPositionPref, localStatePrefs.getBoolean(omniboxPositionPref));
        }

        // Assumes mTab.getProfile() is non-null and UserPrefs.areNativePrefsLoaded is true.
        Profile profile = assumeNonNull(mActivityTabSupplier.get()).getProfile();
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
     * @param preferences The preferences to check.
     * @return whether the user's current settings are different from {@param preferences}.
     */
    private boolean importedSettingsHavePreferenceChange(Map<String, Object> preferences) {
        // Assumes mTab.getProfile() is non-null and UserPrefs.areNativePrefsLoaded is true.
        Profile profile = assumeNonNull(mActivityTabSupplier.get()).getProfile();
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
     * @param preferences The preferences to compare with local.
     * @param onlyOmniboxPosition Whether only the omnibox position should be considered (see
     *     askToApplyNtpSettingImportIfNeeded documentation above).
     * @return Whether the undo/redo snackbar should be shown.
     */
    private boolean shouldShowSnackbar(
            Map<String, Object> preferences, boolean onlyOmniboxPosition) {
        return onlyOmniboxPosition
                ? importedSettingsHaveOmniboxChange(preferences)
                : importedSettingsHavePreferenceChange(preferences);
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
     * @param preferencesToApply The preferences to apply.
     */
    private void applySettings(Map<String, Object> preferencesToApply) {
        applyUserPrefSettings(preferencesToApply);
        applyLocalStateSettings(preferencesToApply);
    }

    /**
     * Applies the user pref settings from {@param preferencesToApply}.
     *
     * @param preferencesToApply The preferences to apply.
     */
    private void applyUserPrefSettings(Map<String, Object> preferencesToApply) {
        // Assumes mTab.getProfile() is non-null and UserPrefs.areNativePrefsLoaded is true.
        Profile profile = assumeNonNull(mActivityTabSupplier.get()).getProfile();
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
     * @param tracker The {@link CrossDevicePrefTracker}.
     * @param profile The {@link Profile}.
     * @return The map of prefs to values.
     */
    @VisibleForTesting
    Map<String, Object> getPrefsFromRemoteDevice(CrossDevicePrefTracker tracker, Profile profile) {
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

    /** Logs UMA with suffix {@param suffix} (omnibox-psecific if {@param onlyOmniboxPosition}) */
    private void recordUma(boolean onlyOmniboxPosition, String suffix) {
        StringBuilder action = new StringBuilder("Android.CrossDeviceSettingImport");
        if (onlyOmniboxPosition) {
            action.append(".OmniboxPosition");
        }
        action.append('.');
        action.append(suffix);
        RecordUserAction.record(action.toString());
    }

    /** Destroys the {@link CrossDeviceSettingImporter}. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityTabSupplier.removeObserver(mTabChangeCallback);
        if (mObservedTab != null) {
            mObservedTab.removeObserver(mTabObserver);
            mObservedTab = null;
        }
    }
}
