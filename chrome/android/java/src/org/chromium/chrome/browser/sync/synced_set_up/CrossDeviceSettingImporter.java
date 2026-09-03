// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS;
import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.ServiceStatus.ACTIVE;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.ServiceStatus.INITIALIZING;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.CrossDeviceThemeTracker;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataImageBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.prefs.CrossDevicePrefTrackerFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
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
import java.util.Objects;
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

    /** Container for settings (preferences and theme) to be synced across devices. */
    @VisibleForTesting
    static class SyncedSetupSettings {
        private final Map<String, Object> mPrefs;
        private final @Nullable NtpBackgroundDataBase mTheme;

        SyncedSetupSettings(Map<String, Object> prefs, @Nullable NtpBackgroundDataBase theme) {
            mPrefs = prefs;
            mTheme = theme;
        }

        SyncedSetupSettings(Map<String, Object> prefs) {
            this(prefs, null);
        }

        Map<String, Object> getPrefs() {
            return mPrefs;
        }

        @Nullable NtpBackgroundDataBase getTheme() {
            return mTheme;
        }

        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) return true;
            if (!(o instanceof SyncedSetupSettings other)) return false;
            return Objects.equals(mPrefs, other.mPrefs) && Objects.equals(mTheme, other.mTheme);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mPrefs, mTheme);
        }

        @Override
        public String toString() {
            return "SyncedSetupSettings{prefs=" + mPrefs + ", theme=" + mTheme + "}";
        }
    }

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
            new TabObserver() {
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
    private @Nullable Runnable mLocalStateObserver;
    private @Nullable CrossDevicePrefTracker mPrefTrackerBeingObserved;
    private @Nullable CrossDevicePrefTrackerObserver mPrefTrackerObserver;
    private @Nullable CrossDeviceThemeTracker mThemeTrackerBeingObserved;
    private CrossDeviceThemeTracker.@Nullable Observer mThemeTrackerObserver;

    private final Callback<@Nullable Tab> mTabChangeCallback =
            (tab) -> {
                if (mObservedTab != null) {
                    mObservedTab.removeObserver(mTabObserver);
                }
                mObservedTab = tab;
                if (mObservedTab != null) {
                    mObservedTab.addObserver(mTabObserver);
                }
                onTabChangeOrGainFocus(tab);
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

    private void stopObservingLocalState() {
        if (mLocalStateObserver != null) {
            LocalStatePrefs.removeObserver(mLocalStateObserver);
        }
        mLocalStateObserver = null;
    }

    private void stopObservingPrefTracker() {
        if (mPrefTrackerObserver != null && mPrefTrackerBeingObserved != null) {
            mPrefTrackerBeingObserved.removeObserver(mPrefTrackerObserver);
        }
        mPrefTrackerObserver = null;
        mPrefTrackerBeingObserved = null;
    }

    private void stopObservingThemeTracker() {
        if (mThemeTrackerObserver != null && mThemeTrackerBeingObserved != null) {
            mThemeTrackerBeingObserved.removeObserver(mThemeTrackerObserver);
        }
        mThemeTrackerObserver = null;
        mThemeTrackerBeingObserved = null;
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
        if (currentTab == null) return;

        @Nullable Profile profile = currentTab.getProfile();
        if (profile == null) return;

        boolean localStateReady = LocalStatePrefs.areNativePrefsLoaded();

        @Nullable CrossDevicePrefTracker crossDevicePrefTracker =
                CrossDevicePrefTrackerFactory.getForProfile(profile);
        if (crossDevicePrefTracker == null) return;
        @ServiceStatus int status = crossDevicePrefTracker.getServiceStatus();
        boolean prefTrackerReady = !NOT_READY_YET_STATES.contains(status);

        @Nullable CrossDeviceThemeTracker crossDeviceThemeTracker = null;
        boolean themeTrackerReady = true;
        if (isThemeFeatureEnabled()) {
            crossDeviceThemeTracker = CrossDeviceThemeTracker.getForProfile(profile);
            if (crossDeviceThemeTracker == null) return;
            int themeStatus = crossDeviceThemeTracker.getServiceStatus();
            themeTrackerReady = themeStatus != INITIALIZING;
        }

        if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(
                    TAG,
                    "onTabChangeOrGainFocus - localStateReady = "
                            + localStateReady
                            + ", prefTrackerReady = "
                            + prefTrackerReady
                            + ", themeTrackerReady = "
                            + themeTrackerReady);
        }

        // If all dependencies are ready, stop any active observation and proceed to import.
        if (localStateReady && prefTrackerReady && themeTrackerReady) {
            stopObservingLocalState();
            stopObservingPrefTracker();
            stopObservingThemeTracker();
            onDependenciesReady(
                    crossDevicePrefTracker, status, profile, currentTab, availableImmediately);
            return;
        }

        // Otherwise, defer the logic by observing whichever dependency is not yet ready.
        if (!localStateReady) {
            ensureObservingLocalState();
        } else {
            stopObservingLocalState();
        }

        if (!prefTrackerReady) {
            ensureObservingPrefTracker(crossDevicePrefTracker, profile);
        } else {
            stopObservingPrefTracker();
        }

        if (isThemeFeatureEnabled() && crossDeviceThemeTracker != null && !themeTrackerReady) {
            ensureObservingThemeTracker(crossDeviceThemeTracker, profile);
        } else {
            stopObservingThemeTracker();
        }
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

    private void ensureObservingPrefTracker(CrossDevicePrefTracker prefTracker, Profile profile) {
        if (mPrefTrackerBeingObserved != null && mPrefTrackerBeingObserved != prefTracker) {
            stopObservingPrefTracker();
        }
        if (mPrefTrackerObserver != null) return;

        mPrefTrackerObserver =
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
        mPrefTrackerBeingObserved = prefTracker;
        prefTracker.addObserver(mPrefTrackerObserver);
    }

    private void ensureObservingThemeTracker(
            CrossDeviceThemeTracker themeTracker, Profile profile) {
        if (mThemeTrackerBeingObserved != null && mThemeTrackerBeingObserved != themeTracker) {
            stopObservingThemeTracker();
        }
        if (mThemeTrackerObserver != null) return;

        mThemeTrackerObserver =
                new CrossDeviceThemeTracker.Observer() {
                    @Override
                    public void onThemesChanged() {}

                    @Override
                    public void onStatusChanged(int status) {
                        // If the tracker is still not ready, keep listening for status changes.
                        if (status == INITIALIZING) {
                            return;
                        }

                        // Ensure the tab and profile are still valid before retrying.
                        @Nullable Tab currentTab = mActivityTabSupplier.get();
                        if (currentTab == null) return;

                        @Nullable Profile currentProfile = currentTab.getProfile();
                        if (!profile.equals(currentProfile)) return;

                        onTabChangeOrGainFocus(currentTab, /* availableImmediately= */ false);
                    }
                };
        mThemeTrackerBeingObserved = themeTracker;
        themeTracker.addObserver(mThemeTrackerObserver);
    }

    /**
     * Handles dependencies reaching a "ready" state.
     *
     * @param tracker The {@link CrossDevicePrefTracker}.
     * @param status The {@link ServiceStatus} of the tracker.
     * @param profile The {@link Profile}.
     * @param tab The {@link Tab} that is currently focused.
     * @param availableImmediately Whether dependencies were available immediately (when we first
     *     checked).
     */
    @VisibleForTesting
    void onDependenciesReady(
            CrossDevicePrefTracker tracker,
            @ServiceStatus int status,
            Profile profile,
            Tab tab,
            boolean availableImmediately) {
        if (ChromeFeatureList.isEnabled(CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(
                    TAG,
                    "running onDependenciesReady with status "
                            + status
                            + ", available immediately ? "
                            + availableImmediately);
        }
        boolean nonNtp = !UrlUtilities.isNtpUrl(tab.getUrl());
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();
        if (nonNtp) {
            if (hasImportedNonNtpSettings(sharedPrefManager)) {
                return;
            }
        } else if (sharedPrefManager.readBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS,
                /* defaultValue= */ true)) {
            return;
        }

        // Record a single action for checking for remote settings, regardless of whether we're
        // handling NTP settings.
        recordAction(/* nonNtp= */ false, "CheckForRemoteSettings");

        // Synced Set Up supports importing settings as long as at least one of Preferences sync
        // or Themes sync is configured and ready on the account.
        boolean prefSyncConfigured = status == ServiceStatus.AVAILABLE;
        boolean themeSyncConfigured = false;
        if (isThemeFeatureEnabled()) {
            @Nullable CrossDeviceThemeTracker themeTracker =
                    CrossDeviceThemeTracker.getForProfile(profile);
            themeSyncConfigured = themeTracker != null && themeTracker.getServiceStatus() == ACTIVE;
        }

        boolean syncConfigured = prefSyncConfigured || themeSyncConfigured;
        if (!syncConfigured) {
            // Neither "Settings" sync nor "Themes" sync is configured in the user's account.
            // We already know that dependencies became "ready", so we are now done.
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.SYNC_NOT_CONFIGURED);
            return;
        }

        Map<String, Object> prefsToApply =
                prefSyncConfigured ? getPrefsFromRemoteDevice(profile, tracker) : Map.of();
        @Nullable NtpBackgroundDataBase candidateTheme = getThemeFromRemoteDevice(profile, tracker);
        SyncedSetupSettings settingsToApply = new SyncedSetupSettings(prefsToApply, candidateTheme);

        Log.i(
                TAG,
                "onDependenciesReady: prefSyncConfigured=%s, themeSyncConfigured=%s,"
                        + " candidateTheme=%s, prefsToApply=%s",
                prefSyncConfigured,
                themeSyncConfigured,
                candidateTheme,
                prefsToApply.keySet());

        if (availableImmediately) {
            // If there was no delay, apply the settings immediately (skipping the user straight
            // to the undo prompt).
            applyAndNotifySettingImport(profile, settingsToApply, /* nonNtp= */ nonNtp);
        } else {
            // If there was a delay, ask the user whether they want to apply the settings.
            askToApplySettingImportIfNeeded(profile, settingsToApply, /* nonNtp= */ nonNtp);
        }
    }

    private static boolean hasImportedNonNtpSettings(SharedPreferencesManager sharedPrefManager) {
        if (sharedPrefManager.contains(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS)) {
            return sharedPrefManager.readBoolean(
                    ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS,
                    /* defaultValue= */ true);
        }
        boolean oldValue =
                sharedPrefManager.readBoolean(
                        ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX,
                        /* defaultValue= */ false);
        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS, oldValue);
        return oldValue;
    }

    /**
     * Marks (possibly only some of the) cross-device setting imports as complete.
     *
     * @param nonNtp Whether only settings that affect non-NTP pages are in scope.
     */
    private static void markCrossDeviceSettingImportComplete(
            boolean nonNtp, @CrossDeviceSettingImportOutcome int reason) {
        recordOutcome(reason);
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();

        sharedPrefManager.writeBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_NON_NTP_SETTINGS, true);
        if (!nonNtp) {
            sharedPrefManager.writeBoolean(
                    ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, true);
        }
    }

    /**
     * Shows {@code snackbar} now if there are no dialogs, or waits until the last dialog is
     * dismissed and then shows it.
     *
     * @param snackbar The {@link Snackbar} to show.
     * @param nonNtp Whether this snackbar only encompasses settings that affect non-NTP pages.
     */
    @VisibleForTesting
    public void showSnackbarAfterDialogs(Snackbar snackbar, boolean nonNtp) {
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
                                    nonNtp, CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
                        }
                    });
        } else {
            snackbarManager.showSnackbar(snackbar);
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to import settings from another device.
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings that will be applied.
     * @param nonNtp Whether only settings that apply to non-NTP pages should be considered. If
     *     true, we only check non-NTP settings to determine whether to show the snackbar, and when
     *     we apply the new settings, only non-NTP settings are applied. If false, all settings are
     *     considered (both for determining whether to show the snackbar and applying the changes).
     */
    @VisibleForTesting
    void askToApplySettingImportIfNeeded(
            Profile profile, SyncedSetupSettings settingsToApply, boolean nonNtp) {
        if (shouldShowSnackbar(profile, settingsToApply, nonNtp)) {
            Snackbar offerApplySnackbar =
                    Snackbar.make(
                            mContext.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    recordAction(nonNtp, "Apply");
                                    applyAndNotifySettingImport(profile, settingsToApply, nonNtp);
                                }
                            },
                            TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_IMPORT);
            offerApplySnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.apply),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerApplySnackbar, nonNtp);
        } else {
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
        }
    }

    @VisibleForTesting
    void askToApplySettingImportIfNeeded(
            Profile profile, Map<String, Object> preferencesToApply, boolean nonNtp) {
        askToApplySettingImportIfNeeded(
                profile, new SyncedSetupSettings(preferencesToApply), nonNtp);
    }

    /**
     * Applies settings from another device and shows a snackbar to the user, informing them that
     * their settings were applied and offering an undo button.
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings that will be applied.
     * @param nonNtp Whether only settings that affect non-NTP pages should be considered (see
     *     askToApplySettingImportIfNeeded documentation above).
     */
    private void applyAndNotifySettingImport(
            Profile profile, SyncedSetupSettings settingsToApply, boolean nonNtp) {
        if (shouldShowSnackbar(profile, settingsToApply, nonNtp)) {
            SyncedSetupSettings currentSettings = getCurrentSettings(profile);
            Snackbar offerUndoSnackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.synced_set_up_snackbar_applied_confirmation),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    boolean hadThemeChange =
                                            importedSettingHasThemeChange(
                                                    settingsToApply.getTheme(),
                                                    currentSettings.getTheme());
                                    Log.i(
                                            TAG,
                                            "offerUndoSnackbar onAction: hadThemeChange=%s,"
                                                    + " currentTheme=%s, settingsToApplyTheme=%s",
                                            hadThemeChange,
                                            currentSettings.getTheme(),
                                            settingsToApply.getTheme());
                                    if (nonNtp) {
                                        applyLocalStateSettings(currentSettings.getPrefs());
                                    } else {
                                        applyUserPrefSettings(profile, currentSettings.getPrefs());
                                        applyLocalStateSettings(currentSettings.getPrefs());
                                    }
                                    if (hadThemeChange) {
                                        applyThemeSettings(currentSettings.getTheme());
                                    }

                                    // If the imported theme was from another Android device (same
                                    // platform) and actually changed the local theme, Android's
                                    // continuous theme sync is active for it. Because the user
                                    // explicitly chose to undo importing this theme, we disable the
                                    // THEMES sync toggle on SyncService so that continuous sync
                                    // does not immediately re-apply the remote Android theme and
                                    // override the user's undo.
                                    // If no theme change occurred, or if the candidate theme was
                                    // cross-platform, disabling the sync toggle is unnecessary.
                                    if (hadThemeChange
                                            && settingsToApply.getTheme() != null
                                            && settingsToApply.getTheme().getPlatformType()
                                                    == PlatformType.ANDROID) {
                                        @Nullable SyncService syncService =
                                                SyncServiceFactory.getForProfile(profile);
                                        if (syncService != null) {
                                            syncService.setSelectedType(
                                                    UserSelectableType.THEMES, false);
                                        }
                                    }

                                    recordAction(nonNtp, "Undo");
                                    askToRedoSettingImport(
                                            profile, settingsToApply, hadThemeChange, nonNtp);
                                }
                            },
                            Snackbar.TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_UNDO);
            offerUndoSnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.undo),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerUndoSnackbar, nonNtp);
            applySettings(profile, settingsToApply);
        } else {
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to redo their setting import (this is offered
     * after the user hits undo).
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings that will be applied during the redo.
     * @param hadThemeChange Whether the imported settings included a theme change.
     * @param nonNtp Whether only settings that affect non-NTP pages should be considered (see
     *     askToApplySettingImportIfNeeded documentation above).
     */
    private void askToRedoSettingImport(
            Profile profile,
            SyncedSetupSettings settingsToApply,
            boolean hadThemeChange,
            boolean nonNtp) {
        Snackbar offerRedoSnackbar =
                Snackbar.make(
                        mContext.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                        new SnackbarManager.SnackbarController() {
                            @Override
                            public void onAction(@Nullable Object actionData) {
                                recordAction(nonNtp, "Redo");
                                // If re-applying an Android candidate theme that had changed the
                                // theme after undo, re-enable the THEMES sync toggle so that
                                // continuous theme sync resumes normally.
                                // It is safe to turn THEMES sync back on because candidate theme
                                // data is only retrieved if the user initially had THEMES sync
                                // enabled prior to undoing.
                                if (hadThemeChange
                                        && settingsToApply.getTheme() != null
                                        && settingsToApply.getTheme().getPlatformType()
                                                == PlatformType.ANDROID) {
                                    @Nullable SyncService syncService =
                                            SyncServiceFactory.getForProfile(profile);
                                    if (syncService != null) {
                                        syncService.setSelectedType(
                                                UserSelectableType.THEMES, true);
                                    }
                                }
                                applyAndNotifySettingImport(profile, settingsToApply, nonNtp);
                            }
                        },
                        TYPE_ACTION,
                        UMA_CROSS_DEVICE_SETTING_REDO);
        offerRedoSnackbar.setAction(
                /* actionText= */ mContext.getString(R.string.redo), /* actionData= */ Map.of());
        showSnackbarAfterDialogs(offerRedoSnackbar, nonNtp);
    }

    /** Returns the user's current settings (including preferences and NTP theme). */
    private SyncedSetupSettings getCurrentSettings(Profile profile) {
        Map<String, Object> prefs = new HashMap<>();

        PrefService localStatePrefs = LocalStatePrefs.get();
        if (localStatePrefs != null) {
            String omniboxPositionPref = Pref.IS_OMNIBOX_IN_BOTTOM_POSITION;
            prefs.put(omniboxPositionPref, localStatePrefs.getBoolean(omniboxPositionPref));
        }

        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs != null) {
            String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
            prefs.put(allCardsPref, userPrefs.getBoolean(allCardsPref));
            for (String key : MODULE_TYPE_TO_USER_PREFS_KEY.values()) {
                prefs.put(key, userPrefs.getBoolean(key));
            }
        }

        // Snapshot current NTP background theme so undo can restore the user's exact prior state.
        @Nullable NtpBackgroundDataBase currentTheme = null;
        if (isThemeFeatureEnabled()) {
            currentTheme = NtpCustomizationConfigManager.getInstance().getNtpBackgroundData();
        }

        return new SyncedSetupSettings(prefs, currentTheme);
    }

    /**
     * @param profile The {@link Profile}.
     * @param settings The settings to check.
     * @return whether the user's current settings are different from {@code settings}.
     */
    /**
     * Returns whether cross-device theme import is enabled and supported on this device. Notably,
     * this checks whether we're in any group besides the control group and that underlying theme
     * sync is supported.
     */
    @VisibleForTesting
    boolean isThemeFeatureEnabled() {
        return ChromeFeatureList.sXplatSyncedSetupThemes.isEnabled()
                && ChromeFeatureList.sNewTabPageCustomizationThemeSync.isEnabled();
    }

    /**
     * Returns whether cross-device theme import is in observation-only mode. In this mode, theme
     * eligibility checks are performed and metrics emitted, but the theme import snackbar is not
     * shown unless non-theme settings also changed.
     */
    @VisibleForTesting
    boolean isObservationOnly() {
        return isThemeFeatureEnabled()
                && ChromeFeatureList.sXplatSyncedSetupThemesObservationOnly.getValue();
    }

    /**
     * Returns whether the snackbar is enabled to show for theme imports. True only when in the
     * enabled group (non-control and not in observation-only mode).
     */
    @VisibleForTesting
    boolean isThemeImportSnackbarEnabled() {
        return isThemeFeatureEnabled() && !isObservationOnly();
    }

    /**
     * @param profile The {@link Profile}.
     * @param prefs The preferences to check.
     * @return whether the user's current preferences are different from {@code prefs}.
     */
    @VisibleForTesting
    boolean importedSettingsHavePreferenceChange(Profile profile, Map<String, Object> prefs) {
        if (!UserPrefs.areNativePrefsLoaded(profile)) return false;

        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs == null) {
            return false;
        }

        String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
        if (importedSettingHasPreferenceChange(prefs, userPrefs, allCardsPref)) {
            return true;
        }

        for (int moduleType : MODULE_TYPE_TO_USER_PREFS_KEY.keySet()) {
            @Nullable String key = MODULE_TYPE_TO_USER_PREFS_KEY.get(moduleType);
            if (key == null) continue;

            if (importedSettingHasPreferenceChange(prefs, userPrefs, key)) return true;
        }

        return importedSettingsAffectNonNtp(prefs);
    }

    /**
     * @param profile The {@link Profile}.
     * @param settings The settings to compare with local.
     * @param nonNtp Whether only settings that affect non-NTP pages should be considered (see
     *     askToApplySettingImportIfNeeded documentation above).
     * @return Whether the undo/redo snackbar should be shown.
     */
    private boolean shouldShowSnackbar(
            Profile profile, SyncedSetupSettings settings, boolean nonNtp) {
        boolean prefChange =
                nonNtp
                        ? importedSettingsAffectNonNtp(settings.getPrefs())
                        : importedSettingsHavePreferenceChange(profile, settings.getPrefs());

        boolean themeChange =
                settings.getTheme() != null
                        && settings.getTheme().getPlatformType() != PlatformType.ANDROID
                        && importedSettingHasThemeChange(settings.getTheme());

        // If in observation-only mode and the user would have been shown the snackbar solely
        // due to a theme change (and not any preference change), record the observation action.
        // Note that the return value at the bottom of this method checks for
        // isThemeImportSnackbarEnabled; this block exists to ensure we record the action at the
        // correct time and does not need a separate return statement.
        if (isObservationOnly() && !prefChange && themeChange) {
            recordAction(nonNtp, "ObservationOnly");
        }

        return prefChange || (isThemeImportSnackbarEnabled() && themeChange);
    }

    /**
     * @param preferences The preferences to check.
     * @return whether the user's settings differ from {@param preferences} in a way that affects
     *     non-NTP pages.
     */
    @VisibleForTesting
    boolean importedSettingsAffectNonNtp(Map<String, Object> preferences) {
        PrefService localPrefs = LocalStatePrefs.get();
        if (localPrefs == null) {
            return false;
        }

        if (preferences.get(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION) instanceof Boolean bottomOmnibox) {
            boolean localBottomOmnibox = localPrefs.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION);
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
                Log.i(
                        TAG,
                        "importedSettingsAffectNonNtp: bottomOmnibox=%s, localBottomOmnibox=%s",
                        bottomOmnibox,
                        localBottomOmnibox);
            }
            if (bottomOmnibox != localBottomOmnibox) {
                return true;
            }
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CROSS_DEVICE_PREF_TRACKER_EXTRA_LOGS)) {
            Log.i(TAG, "importedSettingsAffectNonNtp, returning false at bottom of function");
        }
        return false;
    }

    /**
     * @param settings The settings to check.
     * @return whether the user's settings differ from {@param settings} in a way that affects
     *     non-NTP pages (e.g. bottom omnibox position or omnibox theme coloring).
     */
    @VisibleForTesting
    boolean importedSettingsAffectNonNtp(SyncedSetupSettings settings) {
        if (importedSettingsAffectNonNtp(settings.getPrefs())) {
            return true;
        }

        // Themes affect non-NTP pages as well by tinting the omnibox. Cross-platform themes
        // (which do not sync continuously) that differ from the current local theme therefore
        // affect non-NTP pages.
        if (isThemeImportSnackbarEnabled()
                && settings.getTheme() != null
                && settings.getTheme().getPlatformType() != PlatformType.ANDROID
                && importedSettingHasThemeChange(settings.getTheme())) {
            return true;
        }

        return false;
    }

    /**
     * @param preferences The preferences to check.
     * @param userPrefs The user's current preferences.
     * @param key The key of the preference to check.
     * @return whether the user's current settings are different from {@code preferences} for the
     *     given {@code key}.
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
     * @param candidateTheme The candidate remote theme to evaluate.
     * @param currentTheme The current local theme to compare against.
     * @return whether the candidate remote theme differs from the given local NTP theme.
     */
    private boolean importedSettingHasThemeChange(
            @Nullable NtpBackgroundDataBase candidateTheme,
            @Nullable NtpBackgroundDataBase currentTheme) {
        if (!isThemeFeatureEnabled() || candidateTheme == null) {
            return false;
        }
        return !Objects.equals(candidateTheme, currentTheme);
    }

    /**
     * @param candidateTheme The candidate remote theme to evaluate.
     * @return whether the candidate remote theme differs from the user's current local NTP theme.
     */
    private boolean importedSettingHasThemeChange(@Nullable NtpBackgroundDataBase candidateTheme) {
        @Nullable NtpBackgroundDataBase currentTheme =
                NtpCustomizationConfigManager.getInstance().getNtpBackgroundData();
        return importedSettingHasThemeChange(candidateTheme, currentTheme);
    }

    /**
     * Applies the given {@param settingsToApply}.
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings to apply.
     */
    private void applySettings(Profile profile, SyncedSetupSettings settingsToApply) {
        Log.i(
                TAG,
                "applySettings: prefs=%s, theme=%s",
                settingsToApply.getPrefs().keySet(),
                settingsToApply.getTheme());
        applyUserPrefSettings(profile, settingsToApply.getPrefs());
        applyLocalStateSettings(settingsToApply.getPrefs());
        if (settingsToApply.getTheme() != null) {
            applyThemeSettings(settingsToApply.getTheme());
        }
    }

    /**
     * Applies {@param themeToApply} to the NTP customization manager and persists selection. If
     * {@param themeToApply} is null, clears custom background data back to the default NTP theme.
     */
    private void applyThemeSettings(@Nullable NtpBackgroundDataBase themeToApply) {
        Log.i(
                TAG,
                "applyThemeSettings: themeToApply=%s, isThemeImportSnackbarEnabled=%s",
                themeToApply,
                isThemeImportSnackbarEnabled());
        if (!isThemeImportSnackbarEnabled()) return;

        NtpCustomizationConfigManager configManager = NtpCustomizationConfigManager.getInstance();
        if (themeToApply == null) {
            configManager.onBackgroundDataChanged(mContext, null);
            return;
        }

        if (themeToApply instanceof NtpBackgroundDataColor) {
            configManager.onBackgroundDataChanged(mContext, themeToApply);
            // Persist the user's selected background type so the imported theme survives app
            // restarts.
            configManager.maybeSaveUserSelectedBackgroundTypeToSharedPreference(mContext);
        } else if (themeToApply instanceof NtpBackgroundDataImageBase imageBase) {
            // If the bitmap is null (e.g. from CrossDeviceThemeTracker before downloading),
            // do not write null to configManager. That would clobber the NTP background with
            // null and break rendering. NtpSyncedThemeManager handles the asynchronous download
            // and application of the image.
            // TODO(crbug.com/517615321): Figure out how to handle when the image is downloaded.
            // Currently, when the background image arrives, NtpCustomizationConfigManager triggers
            // an Activity recreate to apply dynamic color theme changes, which causes the active
            // undo/redo snackbar to disappear.
            if (imageBase.getBitmap() != null) {
                configManager.onBackgroundDataChanged(mContext, themeToApply);
                configManager.maybeSaveUserSelectedBackgroundTypeToSharedPreference(mContext);
            }
        }
    }

    /**
     * Applies the user pref settings from {@code preferencesToApply}.
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
     * Applies the local state settings from {@code preferencesToApply}.
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

    /**
     * Retrieves the candidate theme from the best-match remote device (e.g. the template device or
     * most recently active synced device) via {@link CrossDeviceThemeTracker}.
     */
    @VisibleForTesting
    @Nullable NtpBackgroundDataBase getThemeFromRemoteDevice(
            Profile profile, CrossDevicePrefTracker tracker) {
        if (!isThemeFeatureEnabled()) {
            return null;
        }
        @Nullable CrossDeviceThemeTracker themeTracker =
                CrossDeviceThemeTracker.getForProfile(profile);
        if (themeTracker == null) {
            return null;
        }
        @Nullable String bestMatchGuid =
                SyncedSetUpUtilsBridge.getBestMatchDeviceGuid(tracker, profile);
        return themeTracker.getThemeForDeviceGuid(mContext, bestMatchGuid);
    }

    /**
     * Logs UMA with suffix {@param suffix} (if {@param nonNtp}, adds a suffix specifying that we
     * are only working with preferences that affect non-NTP pages).
     */
    private void recordAction(boolean nonNtp, String suffix) {
        StringBuilder action = new StringBuilder("Android.CrossDeviceSettingImport");
        if (nonNtp) {
            action.append(".NonNtp");
        }
        action.append('.');
        action.append(suffix);
        RecordUserAction.record(action.toString());
    }

    @VisibleForTesting
    static final String CROSS_DEVICE_SETTING_IMPORT_OUTCOME_HISTOGRAM =
            "Sync.CrossDeviceSettingImportOutcome";

    /** Logs outcome of cross device setting import (reports showing the feature, or why not. */
    private static void recordOutcome(@CrossDeviceSettingImportOutcome int value) {
        RecordHistogram.recordEnumeratedHistogram(
                CROSS_DEVICE_SETTING_IMPORT_OUTCOME_HISTOGRAM,
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
        stopObservingLocalState();
        stopObservingPrefTracker();
        stopObservingThemeTracker();
    }
}
