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

import android.app.Activity;
import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeManager;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.CrossDeviceThemeTracker;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataImageBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.prefs.CrossDevicePrefTrackerFactory;
import org.chromium.chrome.browser.sync.synced_set_up.SyncedSetUpUtilsBridge.DeviceOsAndFormFactor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.CrossDevicePrefTracker.CrossDevicePrefTrackerObserver;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.ServiceStatus;
import org.chromium.components.sync_preferences.cross_device_pref_tracker.TimestampedPrefValue;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
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

        SyncedSetupSettings rebindContext(Context context) {
            if (mTheme instanceof NtpBackgroundDataColor color) {
                return new SyncedSetupSettings(
                        mPrefs,
                        new NtpBackgroundDataColor(
                                context,
                                color.getPlatformType(),
                                color.getThemeColorId(),
                                color.isChromeColorDailyRefreshEnabled()));
            }
            if (mTheme instanceof NtpBackgroundDataCustomizedColor customColor) {
                return new SyncedSetupSettings(
                        mPrefs,
                        new NtpBackgroundDataCustomizedColor(
                                context,
                                customColor.getPlatformType(),
                                customColor.getPrimaryColorLight(),
                                customColor.getPrimaryColorDark(),
                                customColor.getNtpBackgroundColorLight(),
                                customColor.getNtpBackgroundColorDark()));
            }
            return this;
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

    @VisibleForTesting static final int INVALID_TASK_ID = -1;

    /**
     * Data record ("recipe") for rebuilding an active Apply, Undo, or Redo {@link Snackbar} if the
     * hosting {@link Activity} is destroyed and recreated while the snackbar is on screen.
     *
     * <p>Why this data type is needed:
     *
     * <ul>
     *   <li>Applying or undoing an NTP theme, or an incoming same-platform synced NTP background
     *       completing its download, calls {@link Activity#recreate()} (via {@link
     *       NtpThemeStateProvider#notifyApplyThemeChanges()}), which destroys the current {@link
     *       Activity}, its {@link SnackbarManager}, and the live {@link Snackbar} UI view.
     *   <li>We cannot simply keep the live {@link Snackbar} instance across {@link
     *       Activity#recreate()} because its {@link SnackbarManager.SnackbarController} closure
     *       captures the destroyed {@link Activity}'s {@link Context} and {@link
     *       CrossDeviceSettingImporter} instance.
     *   <li>We also cannot rely on the new {@link Activity} re-running the import from scratch,
     *       because {@link #showSnackbarAfterDialogs} has already marked {@code
     *       CROSS_DEVICE_IMPORTED_*} as {@code true} in {@link SharedPreferencesManager} (and for
     *       Undo/Redo, {@link #applySettings} has already overwritten the user's prior
     *       preferences/theme).
     * </ul>
     *
     * <p>Lifecycle:
     *
     * <ul>
     *   <li>Stored on the current instance in {@link #mActivePendingSnackbar} alongside the live
     *       {@link Snackbar} when {@link #showActionSnackbarAfterDialogs} shows an Apply, Undo, or
     *       Redo snackbar.
     *   <li>If the snackbar finishes normally on the current {@link Activity} (user clicks the
     *       action button or the timeout expires without an activity recreate), {@link
     *       #mActivePendingSnackbar} is discarded.
     *   <li>Only if the {@link Activity} terminates due to a configuration change or {@link
     *       Activity#recreate()} ({@link #isActivityTerminatingForConfigurationChange()}) does
     *       {@link #handleSnackbarDismissOrImporterDestroy()} promote {@link
     *       #mActivePendingSnackbar} to static {@link #sPendingSnackbar}, allowing the recreated
     *       {@link Activity} to rebuild and re-show the snackbar in {@link
     *       #maybeShowPendingSnackbar}.
     * </ul>
     */
    @VisibleForTesting
    static class PendingSnackbar {
        public final boolean isRedo;
        public final @Nullable SyncedSetupSettings previousSettings;
        public final SyncedSetupSettings settingsToApply;
        public final boolean hadThemeChange;
        public final boolean nonNtp;
        public final int taskId;

        /**
         * @param isRedo If true, this is a redo snackbar; otherwise, an undo snackbar (when {@code
         *     previousSettings != null}) or an apply snackbar (when {@code previousSettings ==
         *     null}).
         * @param previousSettings The settings before the import was applied, or null if apply or
         *     redo.
         * @param settingsToApply The settings that will be applied.
         * @param hadThemeChange Whether the imported settings included a theme change.
         * @param nonNtp Whether only settings that affect non-NTP pages should be considered.
         * @param taskId The task ID of the activity where the snackbar was scheduled.
         */
        PendingSnackbar(
                boolean isRedo,
                @Nullable SyncedSetupSettings previousSettings,
                SyncedSetupSettings settingsToApply,
                boolean hadThemeChange,
                boolean nonNtp,
                int taskId) {
            this.isRedo = isRedo;
            this.previousSettings = previousSettings;
            this.settingsToApply = settingsToApply;
            this.hadThemeChange = hadThemeChange;
            this.nonNtp = nonNtp;
            this.taskId = taskId;
        }
    }

    private static @Nullable PendingSnackbar sPendingSnackbar;

    private static void setPendingSnackbar(@Nullable PendingSnackbar pendingSnackbar) {
        sPendingSnackbar = pendingSnackbar;
        if (pendingSnackbar != null) {
            ResettersForTesting.register(() -> sPendingSnackbar = null);
        }
    }

    static @Nullable PendingSnackbar getPendingSnackbarForTesting() {
        return sPendingSnackbar;
    }

    static void setPendingSnackbarForTesting(@Nullable PendingSnackbar pendingSnackbar) {
        setPendingSnackbar(pendingSnackbar);
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

                @Override
                public void onDestroyed(Tab tab) {
                    if (mObservedTab == tab) {
                        mObservedTab.removeObserver(mTabObserver);
                        mObservedTab = null;
                    }
                }
            };

    private @Nullable Tab mObservedTab;
    private @Nullable Runnable mLocalStateObserver;
    private @Nullable CrossDevicePrefTracker mPrefTrackerBeingObserved;
    private @Nullable CrossDevicePrefTrackerObserver mPrefTrackerObserver;
    private @Nullable CrossDeviceThemeTracker mThemeTrackerBeingObserved;
    private CrossDeviceThemeTracker.@Nullable Observer mThemeTrackerObserver;
    private @Nullable ModalDialogManager mModalDialogManagerBeingObserved;
    private @Nullable ModalDialogManagerObserver mModalDialogObserver;
    private @Nullable PendingSnackbar mActivePendingSnackbar;
    private boolean mIsRestoringPendingSnackbar;
    private boolean mIsCrossOsThemeFetchInFlight;
    // The `nonNtp` scope for the active cross-OS wallpaper download (only meaningful while
    // `mIsCrossOsThemeFetchInFlight` is true; upgraded from true to false if the user switches to
    // an NTP mid-download).
    private boolean mInFlightFetchNonNtp;
    private boolean mIsDestroyed;

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
     * Returns whether {@link CrossDeviceSettingImporter} has any pending import or snackbar work
     * and should be instantiated.
     */
    public static boolean shouldCreateImporter() {
        return sPendingSnackbar != null
                || !hasImportedAllSettings(ChromeSharedPreferences.getInstance());
    }

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

    private void ensureNtpCustomizationConfigManagerInitialized() {
        if (!isThemeFeatureEnabled()) return;
        // NtpCustomizationConfigManager defers initializing CHROME_COLOR and COLOR_FROM_HEX
        // themes until a Context is provided. Calling ensureInitialized() here when an import or
        // pending snackbar is active ensures the manager is initialized even on non-NTP pages and
        // on tablets (where StatusBarColorController does not register a listener), without
        // loading NTP themes on launches where settings have already been imported.
        NtpCustomizationConfigManager.getInstance().ensureInitialized(mContext);
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

    private void stopObservingModalDialogManager() {
        if (mModalDialogObserver != null && mModalDialogManagerBeingObserved != null) {
            mModalDialogManagerBeingObserved.removeObserver(mModalDialogObserver);
        }
        mModalDialogObserver = null;
        mModalDialogManagerBeingObserved = null;
    }

    @VisibleForTesting
    int getTaskId() {
        if (mContext instanceof Activity activity) {
            return ApplicationStatus.getTaskId(activity);
        }
        return INVALID_TASK_ID;
    }

    @VisibleForTesting
    boolean matchesCurrentTask(int pendingTaskId) {
        int currentTaskId = getTaskId();
        if (pendingTaskId == INVALID_TASK_ID || currentTaskId == INVALID_TASK_ID) {
            return false;
        }
        return pendingTaskId == currentTaskId;
    }

    /**
     * Returns whether the current activity is being terminated due to a configuration change.
     *
     * <p>In Android, {@link Activity#isChangingConfigurations()} returns true whenever the activity
     * is being destroyed and recreated due to configuration changes (such as screen rotation,
     * display size/foldable state changes, or UI mode changes) as well as programmatic {@link
     * Activity#recreate()} calls (such as applying or undoing a dynamic theme change).
     */
    private boolean isActivityTerminatingForConfigurationChange() {
        return mContext instanceof Activity activity && activity.isChangingConfigurations();
    }

    /**
     * Checks if a snackbar presentation is pending after an activity recreation and displays it.
     *
     * @param profile The current active {@link Profile}.
     * @return Whether a pending snackbar was restored and displayed.
     */
    @VisibleForTesting
    boolean maybeShowPendingSnackbar(Profile profile) {
        if (sPendingSnackbar == null) return false;
        if (!matchesCurrentTask(sPendingSnackbar.taskId)) return false;
        SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
        if (mModalDialogManagerSupplier.get() == null || snackbarManager == null) {
            return false;
        }

        ensureNtpCustomizationConfigManagerInitialized();
        PendingSnackbar pending = sPendingSnackbar;
        setPendingSnackbar(null);
        SyncedSetupSettings settingsToApply = pending.settingsToApply.rebindContext(mContext);
        mIsRestoringPendingSnackbar = true;
        if (pending.isRedo) {
            showOfferRedoSnackbarAfterDialogs(
                    profile, settingsToApply, pending.hadThemeChange, pending.nonNtp);
        } else if (pending.previousSettings != null) {
            SyncedSetupSettings previousSettings = pending.previousSettings.rebindContext(mContext);
            settingsToApply = maybeUpdateSettingsWithDownloadedTheme(settingsToApply);
            showOfferUndoSnackbarAfterDialogs(
                    profile, previousSettings, settingsToApply, pending.nonNtp);
        } else {
            showOfferApplySnackbarAfterDialogs(profile, settingsToApply, pending.nonNtp);
        }
        mIsRestoringPendingSnackbar = false;
        return true;
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
        if (profile == null || profile.isOffTheRecord()) return;

        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();
        boolean isCurrentTabNtp = UrlUtilities.isNtpUrl(currentTab.getUrl());

        if (sPendingSnackbar != null && matchesCurrentTask(sPendingSnackbar.taskId)) {
            boolean wasNonNtp = sPendingSnackbar.nonNtp;
            // Show the pending snackbar if the task ID matches.
            //
            // It is safe to restore the pending snackbar before checking tracker dependencies:
            // the snackbar payload is already preserved in memory in PendingSnackbar, and
            // process/profile-scoped dependencies (native prefs, sync trackers) remain initialized
            // across activity restarts.
            if (!maybeShowPendingSnackbar(profile)) {
                return;
            }
            boolean needsNtpImport =
                    isCurrentTabNtp && wasNonNtp && !hasImportedAllSettings(sharedPrefManager);
            if (!needsNtpImport) {
                return;
            }
        }

        boolean nonNtp = !isCurrentTabNtp;
        if (mIsCrossOsThemeFetchInFlight) {
            // Avoid showing a duplicate Offer Apply snackbar while the wallpaper download is in
            // flight. If the user navigated to an NTP, widen the in-flight import to include NTP
            // card prefs once the download finishes.
            if (!nonNtp) {
                mInFlightFetchNonNtp = false;
            }
            return;
        }
        if (nonNtp
                ? hasImportedNonNtpSettings(sharedPrefManager)
                : hasImportedAllSettings(sharedPrefManager)) {
            return;
        }

        ensureNtpCustomizationConfigManagerInitialized();

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
                    "onTabChangeOrGainFocus - localStateReady = %s, prefTrackerReady = %s,"
                            + " themeTrackerReady = %s",
                    localStateReady,
                    prefTrackerReady,
                    themeTrackerReady);
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
                    stopObservingLocalState();
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
                    "running onDependenciesReady with status %s, available immediately ? %s",
                    status,
                    availableImmediately);
        }
        boolean nonNtp = !UrlUtilities.isNtpUrl(tab.getUrl());
        SharedPreferencesManager sharedPrefManager = ChromeSharedPreferences.getInstance();
        if (nonNtp
                ? hasImportedNonNtpSettings(sharedPrefManager)
                : hasImportedAllSettings(sharedPrefManager)) {
            return;
        }

        ensureNtpCustomizationConfigManagerInitialized();

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

        if (availableImmediately && !needsCrossOsThemeImageFetch(candidateTheme)) {
            // If there was no delay, apply the settings immediately (skipping the user straight
            // to the undo prompt).
            applyAndNotifySettingImport(profile, settingsToApply, /* nonNtp= */ nonNtp);
        } else {
            // If there was a delay (or a cross-OS background image needs user confirmation before
            // downloading), ask the user whether they want to apply the settings.
            askToApplySettingImportIfNeeded(profile, settingsToApply, /* nonNtp= */ nonNtp);
        }
    }

    private static boolean hasImportedAllSettings(SharedPreferencesManager sharedPrefManager) {
        return sharedPrefManager.readBoolean(
                ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_ALL_SETTINGS, /* defaultValue= */ true);
    }

    private static boolean hasImportedNonNtpSettings(SharedPreferencesManager sharedPrefManager) {
        if (hasImportedAllSettings(sharedPrefManager)) {
            return true;
        }
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
        sharedPrefManager.removeKey(ChromePreferenceKeys.CROSS_DEVICE_IMPORTED_BOTTOM_OMNIBOX);
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

        stopObservingModalDialogManager();

        if (modalDialogManager.isShowing()) {
            mModalDialogManagerBeingObserved = modalDialogManager;
            mModalDialogObserver =
                    new ModalDialogManagerObserver() {
                        @Override
                        public void onLastDialogDismissed() {
                            modalDialogManager.removeObserver(this);
                            mModalDialogObserver = null;
                            mModalDialogManagerBeingObserved = null;
                            snackbarManager.showSnackbar(snackbar);
                            markCrossDeviceSettingImportComplete(
                                    nonNtp, CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
                        }
                    };
            modalDialogManager.addObserver(mModalDialogObserver);
        } else {
            snackbarManager.showSnackbar(snackbar);
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.SNACKBAR_SHOWN);
        }
    }

    /**
     * Constructs and displays an action {@link Snackbar} on the current {@link Activity} (waiting
     * for any active modal dialogs to dismiss first), while optionally arming a {@link
     * PendingSnackbar} backup recipe in {@link #mActivePendingSnackbar} in case the current {@link
     * Activity} is recreated before the snackbar finishes.
     *
     * <p>Why both a live {@link Snackbar} and {@code pendingSnackbar} are used:
     *
     * <ul>
     *   <li>If no {@link Activity} recreation occurs (e.g. when only non-theme preferences such as
     *       omnibox position or NTP cards are applied), the live {@link Snackbar} created here
     *       stays on screen for its normal duration and {@code pendingSnackbar} is discarded when
     *       it dismisses.
     *   <li>If an {@link Activity} recreation occurs while the snackbar is active (either
     *       immediately when {@link #applyThemeSettings} calls {@link Activity#recreate()}, or
     *       later if the user rotates the device), the live {@link Snackbar} is destroyed along
     *       with the old {@link Activity}. The {@link SnackbarManager.SnackbarController} below
     *       detects {@link #isActivityTerminatingForConfigurationChange()} inside {@link
     *       #handleSnackbarDismissOrImporterDestroy()} and promotes {@code pendingSnackbar} to
     *       {@link #sPendingSnackbar} so the new {@link Activity} can construct a fresh {@link
     *       Snackbar}.
     * </ul>
     *
     * @param messageResId String resource ID for the snackbar body text.
     * @param actionResId String resource ID for the snackbar action button ("Apply", "Undo",
     *     "Redo").
     * @param umaIdentifier UMA snackbar identifier.
     * @param onAction Callback invoked when the user clicks the snackbar action button.
     * @param nonNtp Whether this snackbar is scoped to non-NTP settings.
     * @param pendingSnackbar Backup state recipe for rebuilding this snackbar if the {@link
     *     Activity} is recreated while it is showing, or {@code null} if no valid task ID is
     *     available.
     */
    private void showActionSnackbarAfterDialogs(
            @StringRes int messageResId,
            @StringRes int actionResId,
            int umaIdentifier,
            Runnable onAction,
            boolean nonNtp,
            @Nullable PendingSnackbar pendingSnackbar) {
        if (mModalDialogManagerSupplier.get() == null || mSnackbarManagerSupplier.get() == null) {
            return;
        }
        mActivePendingSnackbar = pendingSnackbar;
        Snackbar snackbar =
                Snackbar.make(
                                mContext.getString(messageResId),
                                new SnackbarManager.SnackbarController() {
                                    @Override
                                    public void onAction(@Nullable Object actionData) {
                                        mActivePendingSnackbar = null;
                                        setPendingSnackbar(null);
                                        onAction.run();
                                    }

                                    @Override
                                    public void onDismissNoAction(@Nullable Object actionData) {
                                        handleSnackbarDismissOrImporterDestroy();
                                    }
                                },
                                TYPE_ACTION,
                                umaIdentifier)
                        .setAction(mContext.getString(actionResId), Map.of())
                        .setAnimateIn(!mIsRestoringPendingSnackbar);
        showSnackbarAfterDialogs(snackbar, nonNtp);
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
            showOfferApplySnackbarAfterDialogs(profile, settingsToApply, nonNtp);
        } else {
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
        }
    }

    @VisibleForTesting
    void showOfferApplySnackbarAfterDialogs(
            Profile profile, SyncedSetupSettings settingsToApply, boolean nonNtp) {
        int taskId = getTaskId();
        Context contextToRebind = getContextForPendingSnackbar();
        @Nullable PendingSnackbar pendingSnackbar =
                taskId != INVALID_TASK_ID
                        ? new PendingSnackbar(
                                /* isRedo= */ false,
                                /* previousSettings= */ null,
                                settingsToApply.rebindContext(contextToRebind),
                                /* hadThemeChange= */ false,
                                nonNtp,
                                taskId)
                        : null;
        showActionSnackbarAfterDialogs(
                R.string.synced_set_up_snackbar_ask_to_apply,
                R.string.apply,
                UMA_CROSS_DEVICE_SETTING_IMPORT,
                () -> {
                    recordAction(nonNtp, "Apply");
                    applyAndNotifySettingImport(profile, settingsToApply, nonNtp);
                },
                nonNtp,
                pendingSnackbar);
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
        if (!shouldShowSnackbar(profile, settingsToApply, nonNtp)) {
            markCrossDeviceSettingImportComplete(
                    nonNtp, CrossDeviceSettingImportOutcome.NO_SETTINGS_TO_IMPORT);
            return;
        }

        if (settingsToApply.getTheme()
                        instanceof NtpBackgroundDataThemeCollection candidateCollection
                && needsCrossOsThemeImageFetch(candidateCollection)) {
            fetchAndApplyCrossOsThemeImage(profile, settingsToApply, candidateCollection, nonNtp);
            return;
        }

        SyncedSetupSettings currentSettings =
                getCurrentSettings(profile, settingsToApply.getTheme());
        showOfferUndoSnackbarAfterDialogs(profile, currentSettings, settingsToApply, nonNtp);
        applySettings(profile, settingsToApply, nonNtp);
    }

    /**
     * Returns whether {@code candidateTheme} is a cross-OS wallpaper that must be downloaded on
     * user confirmation before it can be applied. Unlike Android-to-Android theme sync (where
     * {@link NtpSyncedThemeManager} downloads the bitmap in the background), cross-OS theme
     * collections from {@link CrossDeviceThemeTracker} arrive with a {@code null} bitmap.
     *
     * @param candidateTheme The candidate theme to check.
     * @return Whether {@code candidateTheme} requires an asynchronous cross-OS image download.
     */
    private boolean needsCrossOsThemeImageFetch(@Nullable NtpBackgroundDataBase candidateTheme) {
        return isThemeImportSnackbarEnabled()
                && candidateTheme instanceof NtpBackgroundDataThemeCollection candidateCollection
                && candidateCollection.getBitmap() == null
                && candidateCollection.getPlatformType() != PlatformType.ANDROID
                && importedSettingHasThemeChange(
                        candidateCollection, getCurrentLocalTheme(candidateCollection));
    }

    /**
     * Asynchronously downloads the wallpaper image for a cross-OS {@link
     * NtpBackgroundDataThemeCollection} and, once complete, applies the resolved settings (or falls
     * back to applying only non-theme preferences if the image cannot be fetched) and shows the
     * Undo snackbar.
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings to apply once the theme image is resolved.
     * @param candidateCollection The cross-OS theme collection whose image needs to be downloaded.
     * @param nonNtp Whether only settings that affect non-NTP pages should be considered when the
     *     fetch starts (widened to {@code false} if the user navigates to an NTP mid-download).
     */
    private void fetchAndApplyCrossOsThemeImage(
            Profile profile,
            SyncedSetupSettings settingsToApply,
            NtpBackgroundDataThemeCollection candidateCollection,
            boolean nonNtp) {
        CustomBackgroundInfo info = candidateCollection.getCustomBackgroundInfo();
        if (info == null
                || info.backgroundUrl == null
                || !info.backgroundUrl.isValid()
                || info.backgroundUrl.isEmpty()) {
            // Fall back to importing non-theme preferences without clobbering the local wallpaper.
            applyAndNotifySettingImport(
                    profile,
                    new SyncedSetupSettings(settingsToApply.getPrefs(), /* theme= */ null),
                    nonNtp);
            return;
        }
        @Nullable ImageFetcher imageFetcher = NtpCustomizationUtils.createImageFetcher(profile);
        if (imageFetcher == null) {
            applyAndNotifySettingImport(
                    profile,
                    new SyncedSetupSettings(settingsToApply.getPrefs(), /* theme= */ null),
                    nonNtp);
            return;
        }

        mIsCrossOsThemeFetchInFlight = true;
        mInFlightFetchNonNtp = nonNtp;
        NtpCustomizationUtils.fetchThemeCollectionImage(
                imageFetcher,
                info.backgroundUrl,
                (bitmap) -> {
                    imageFetcher.destroy();
                    mIsCrossOsThemeFetchInFlight = false;
                    if (mIsDestroyed) return;
                    // `fetchThemeCollectionImage` is async: `onTabChangeOrGainFocus` may have
                    // updated `mInFlightFetchNonNtp` to false while the download was in flight.
                    boolean effectiveNonNtp = mInFlightFetchNonNtp;
                    @Nullable NtpBackgroundDataThemeCollection resolvedTheme = null;
                    if (bitmap != null) {
                        BackgroundImageInfo backgroundImageInfo =
                                NtpCustomizationUtils.getDefaultBackgroundImageInfo(
                                        mContext, bitmap);
                        @Nullable
                        @ColorInt
                        Integer primaryColor =
                                candidateCollection.getPrimaryColor() != null
                                        ? candidateCollection.getPrimaryColor()
                                        : NtpCustomizationUtils.getContentBasedSeedColor(bitmap);
                        @Nullable String fileIdHash =
                                NtpCustomizationUtils.getFileName(info.backgroundUrl.getPath());
                        // TODO(crbug.com/517615321): Preserve seed color and browser color variant
                        // from candidateCollection (or set UNSPECIFIED when extracted from bitmap)
                        // once NtpBackgroundDataThemeCollection stores them.
                        resolvedTheme =
                                new NtpBackgroundDataThemeCollection(
                                        candidateCollection.getPlatformType(),
                                        info,
                                        backgroundImageInfo,
                                        bitmap,
                                        primaryColor,
                                        fileIdHash);
                    }
                    // Re-enter with the decoded bitmap (or null theme if the fetch failed) so
                    // needsCrossOsThemeImageFetch is false and the Undo snackbar caches the
                    // resolved bitmap for Redo.
                    applyAndNotifySettingImport(
                            profile,
                            new SyncedSetupSettings(settingsToApply.getPrefs(), resolvedTheme),
                            effectiveNonNtp);
                });
    }

    private Context getContextForPendingSnackbar() {
        Context appContext = mContext.getApplicationContext();
        return appContext != null ? appContext : mContext;
    }

    /**
     * Returns a copy of {@code settingsToApply} whose theme is enriched with the decoded {@link
     * android.graphics.Bitmap} from {@link NtpCustomizationConfigManager} if an async theme image
     * download has completed.
     *
     * <p>Why this is needed:
     *
     * <ul>
     *   <li>{@link CrossDeviceThemeTracker} only reads C++ {@code DeviceInfo} sync specifics (URL
     *       and collection metadata) and never holds a {@link android.graphics.Bitmap}, so {@code
     *       settingsToApply.getTheme()} initially has {@code getBitmap() == null}.
     *   <li>During initial import, {@link NtpSyncedThemeManager} downloads the wallpaper bitmap
     *       asynchronously and applies it the first time.
     *   <li>However, if the user later taps <b>Undo</b> (reverting to {@code previousSettings}) and
     *       then taps <b>Redo</b>, {@code NtpSyncedThemeManager} will not download the wallpaper a
     *       second time — {@link #applyThemeSettings} must re-apply {@code
     *       settingsToApply.getTheme()} directly, which requires {@code getBitmap() != null}.
     * </ul>
     */
    private SyncedSetupSettings maybeUpdateSettingsWithDownloadedTheme(
            SyncedSetupSettings settingsToApply) {
        @Nullable NtpBackgroundDataBase enrichedTheme =
                maybeEnrichThemeWithDownloadedBitmap(
                        settingsToApply.getTheme(),
                        getCurrentLocalTheme(/* incomingTheme= */ null));
        if (enrichedTheme == null) return settingsToApply;
        return new SyncedSetupSettings(settingsToApply.getPrefs(), enrichedTheme);
    }

    /**
     * Returns the current local NTP background theme. When importing a cross-platform theme,
     * prefers any Android-synced theme staged via {@link
     * NtpCustomizationConfigManager#getSyncedNtpBackgroundData()} before {@link
     * NtpCustomizationConfigManager#maybeApplyBackgroundUpdateFromDeviceSync} promotes it to {@link
     * NtpCustomizationConfigManager#getNtpBackgroundData()}. When importing a same-platform ({@link
     * PlatformType#ANDROID}) theme, {@code getSyncedNtpBackgroundData()} holds the incoming remote
     * Android theme itself rather than the prior local theme, so {@code getNtpBackgroundData()} is
     * used directly.
     */
    private @Nullable NtpBackgroundDataBase getCurrentLocalTheme(
            @Nullable NtpBackgroundDataBase incomingTheme) {
        if (!isThemeFeatureEnabled()) return null;

        ensureNtpCustomizationConfigManagerInitialized();
        NtpCustomizationConfigManager configManager = NtpCustomizationConfigManager.getInstance();

        // When NtpSyncedThemeManager finishes downloading a wallpaper, it first stages the
        // NtpBackgroundDataThemeCollection (with its decoded Bitmap) in mSyncedNtpBackgroundData
        // via onSyncedThemeCollectionImageChanged(). Later,
        // maybeApplyBackgroundUpdateFromDeviceSync() promotes that object into mNtpBackgroundData,
        // clears mSyncedNtpBackgroundData to null, and triggers an Activity recreate:
        // - Before maybeApplyBackgroundUpdateFromDeviceSync() runs, the downloaded theme is in
        //   getSyncedNtpBackgroundData().
        // - After maybeApplyBackgroundUpdateFromDeviceSync() runs (and recreates the Activity),
        //   the downloaded theme is in getNtpBackgroundData().
        boolean isSamePlatformImport =
                incomingTheme != null && incomingTheme.getPlatformType() == PlatformType.ANDROID;
        @Nullable NtpBackgroundDataBase syncedTheme = configManager.getSyncedNtpBackgroundData();
        if (!isSamePlatformImport && syncedTheme != null) {
            return syncedTheme;
        }
        return configManager.getNtpBackgroundData();
    }

    /**
     * Enriches a metadata-only target theme collection ({@code getBitmap() == null}) with the
     * decoded {@link android.graphics.Bitmap}, image matrices, file hash, and extracted seed color
     * from {@code downloadedTheme} when both refer to the same theme collection wallpaper.
     *
     * <p>Note on platform behavior and assumptions:
     *
     * <ul>
     *   <li><b>{@link PlatformType#ANDROID}</b>: {@link CrossDeviceSettingImporter} may show the
     *       Undo snackbar immediately while {@link NtpSyncedThemeManager} is still downloading the
     *       wallpaper in the background. At that time, {@code targetTheme} has {@code getBitmap()
     *       == null} and {@code getPrimaryColor() == null}, and this method enriches it once {@link
     *       NtpSyncedThemeManager} finishes downloading the bitmap and extracting its seed color.
     *   <li><b>{@link PlatformType#DESKTOP}</b>: Desktop's {@code ThemeSpecifics} proto already
     *       embeds {@code user_color_theme} (and {@code ntp_background.main_color}) once Desktop
     *       extracts the thumbnail color (or when the user selects a custom color), and cross-OS
     *       wallpapers download their bitmap and resolve {@code primaryColor} on Apply before the
     *       Undo snackbar is created.
     *   <li><b>{@link PlatformType#IOS}</b>: iOS's {@code HomeBackgroundCustomizationService}
     *       clears {@code user_color_theme} when setting {@code ntp_background} and does not set
     *       {@code main_color}, so {@code ThemeIosSpecifics} wallpapers arrive with {@code
     *       getPrimaryColor() == null}; however, like Desktop, cross-OS wallpapers download their
     *       bitmap and compute {@code getContentBasedSeedColor(bitmap)} on Apply before the Undo
     *       snackbar is created.
     * </ul>
     *
     * Thus, in practice this method only enriches in-flight {@link PlatformType#ANDROID} downloads,
     * though it defensively preserves {@code targetTheme}'s {@code platformType} and any non-null
     * {@code primaryColor}.
     *
     * @param targetTheme The candidate theme from {@link SyncedSetupSettings#getTheme()}, created
     *     with {@code getBitmap() == null}.
     * @param downloadedTheme The active or staged theme from {@link NtpCustomizationConfigManager}
     *     containing the downloaded {@link android.graphics.Bitmap}.
     * @return A new {@link NtpBackgroundDataThemeCollection} with {@code downloadedTheme}'s bitmap
     *     and local file metadata, or {@code null} if {@code targetTheme} does not match or already
     *     has a bitmap.
     */
    private static @Nullable NtpBackgroundDataThemeCollection maybeEnrichThemeWithDownloadedBitmap(
            @Nullable NtpBackgroundDataBase targetTheme,
            @Nullable NtpBackgroundDataBase downloadedTheme) {
        if (downloadedTheme == null) return null;
        if (!(targetTheme instanceof NtpBackgroundDataThemeCollection targetCollection)) {
            return null;
        }
        if (!(downloadedTheme instanceof NtpBackgroundDataThemeCollection downloadedCollection)) {
            return null;
        }
        if (targetCollection.getBitmap() != null || downloadedCollection.getBitmap() == null) {
            return null;
        }

        // Match the same wallpaper with a compatible primary color. targetCollection's color is
        // null on an in-flight Android download, while downloadedCollection's color is non-null
        // once NtpSyncedThemeManager extracts the seed color from the bitmap. Only reject when
        // both have a non-null primary color and they differ.
        if (!targetCollection.hasSameThemeAndCompatibleColor(downloadedCollection)) {
            return null;
        }

        // Construct a new NtpBackgroundDataThemeCollection rather than mutating targetCollection in
        // place:
        // 1. Preserves targetCollection's mPlatformType and mCustomBackgroundInfo.
        // 2. Avoids changing the bitmap and primary color of the instance still held by the
        //    original SyncedSetupSettings or any cached reference.
        // TODO(crbug.com/517615321): Preserve seed color and browser color variant from
        // targetCollection/downloadedCollection once NtpBackgroundDataThemeCollection stores them.
        return new NtpBackgroundDataThemeCollection(
                targetCollection.getPlatformType(),
                targetCollection.getCustomBackgroundInfo(),
                downloadedCollection.getBackgroundImageInfo(),
                downloadedCollection.getBitmap(),
                targetCollection.getPrimaryColor() != null
                        ? targetCollection.getPrimaryColor()
                        : downloadedCollection.getPrimaryColor(),
                downloadedCollection.getFileIdHash());
    }

    @VisibleForTesting
    void showOfferUndoSnackbarAfterDialogs(
            Profile profile,
            SyncedSetupSettings currentSettings,
            SyncedSetupSettings settingsToApply,
            boolean nonNtp) {
        int taskId = getTaskId();
        Context contextToRebind = getContextForPendingSnackbar();
        @Nullable PendingSnackbar pendingSnackbar =
                taskId != INVALID_TASK_ID
                        ? new PendingSnackbar(
                                /* isRedo= */ false,
                                currentSettings.rebindContext(contextToRebind),
                                settingsToApply.rebindContext(contextToRebind),
                                /* hadThemeChange= */ false,
                                nonNtp,
                                taskId)
                        : null;
        showActionSnackbarAfterDialogs(
                R.string.synced_set_up_snackbar_applied_confirmation,
                R.string.undo,
                UMA_CROSS_DEVICE_SETTING_UNDO,
                () -> {
                    SyncedSetupSettings updatedSettings =
                            maybeUpdateSettingsWithDownloadedTheme(settingsToApply);
                    boolean hadThemeChange =
                            importedSettingHasThemeChange(
                                    updatedSettings.getTheme(), currentSettings.getTheme());
                    Log.i(
                            TAG,
                            "offerUndoSnackbar onAction: hadThemeChange=%s,"
                                    + " currentTheme=%s, settingsToApplyTheme=%s",
                            hadThemeChange,
                            currentSettings.getTheme(),
                            updatedSettings.getTheme());
                    if (nonNtp) {
                        applyLocalStateSettings(currentSettings.getPrefs());
                    } else {
                        applyUserPrefSettings(profile, currentSettings.getPrefs());
                        applyLocalStateSettings(currentSettings.getPrefs());
                    }

                    // If the imported theme was from another Android device (same platform) and
                    // actually changed the local theme, Android's continuous theme sync is active
                    // for it. Because the user explicitly chose to undo importing this theme, we
                    // disable the THEMES sync toggle on SyncService so that continuous sync does
                    // not immediately re-apply the remote Android theme and override the user's
                    // undo. If no theme change occurred, or if the candidate theme was
                    // cross-platform, disabling the sync toggle is unnecessary.
                    if (hadThemeChange
                            && updatedSettings.getTheme() != null
                            && updatedSettings.getTheme().getPlatformType()
                                    == PlatformType.ANDROID) {
                        @Nullable SyncService syncService =
                                SyncServiceFactory.getForProfile(profile);
                        if (syncService != null) {
                            syncService.setSelectedType(UserSelectableType.THEMES, false);
                        }
                    }

                    recordAction(nonNtp, "Undo");
                    showOfferRedoSnackbarAfterDialogs(
                            profile, updatedSettings, hadThemeChange, nonNtp);
                    if (hadThemeChange) {
                        if (updatedSettings.getTheme()
                                instanceof NtpBackgroundDataImageBase imageBase) {
                            // Reset isBitmapSaved to false when Undo deletes the saved image file
                            // on disk, so that a subsequent Redo will re-persist the bitmap file to
                            // disk in saveBackgroundInfo().
                            imageBase.setIsBitmapSaved(false);
                        }
                        if (currentSettings.getTheme()
                                instanceof NtpBackgroundDataImageBase currentImageBase) {
                            // Applying a cross-OS theme calls
                            // clearPendingSyncedBackgroundAndSharedPreference(), which deletes a
                            // staged Android-synced theme's image file on disk while leaving
                            // isBitmapSaved() == true in memory. Resetting isBitmapSaved to false
                            // ensures Undo re-persists the restored bitmap file to disk.
                            currentImageBase.setIsBitmapSaved(false);
                        }
                        applyThemeSettings(currentSettings.getTheme());
                    }
                },
                nonNtp,
                pendingSnackbar);
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
    @VisibleForTesting
    void showOfferRedoSnackbarAfterDialogs(
            Profile profile,
            SyncedSetupSettings settingsToApply,
            boolean hadThemeChange,
            boolean nonNtp) {
        int taskId = getTaskId();
        Context contextToRebind = getContextForPendingSnackbar();
        @Nullable PendingSnackbar pendingSnackbar =
                taskId != INVALID_TASK_ID
                        ? new PendingSnackbar(
                                /* isRedo= */ true,
                                /* previousSettings= */ null,
                                settingsToApply.rebindContext(contextToRebind),
                                hadThemeChange,
                                nonNtp,
                                taskId)
                        : null;
        showActionSnackbarAfterDialogs(
                R.string.synced_set_up_snackbar_removed_confirmation,
                R.string.redo,
                UMA_CROSS_DEVICE_SETTING_REDO,
                () -> {
                    recordAction(nonNtp, "Redo");
                    // If re-applying an Android candidate theme that had changed the theme after
                    // undo, re-enable the THEMES sync toggle so that continuous theme sync resumes
                    // normally. It is safe to turn THEMES sync back on because candidate theme data
                    // is only retrieved if the user initially had THEMES sync enabled prior to
                    // undoing.
                    if (hadThemeChange
                            && settingsToApply.getTheme() != null
                            && settingsToApply.getTheme().getPlatformType()
                                    == PlatformType.ANDROID) {
                        @Nullable SyncService syncService =
                                SyncServiceFactory.getForProfile(profile);
                        if (syncService != null) {
                            syncService.setSelectedType(UserSelectableType.THEMES, true);
                        }
                    }
                    applyAndNotifySettingImport(profile, settingsToApply, nonNtp);
                },
                nonNtp,
                pendingSnackbar);
    }

    /** Returns the user's current settings (including preferences and NTP theme). */
    private SyncedSetupSettings getCurrentSettings(
            Profile profile, @Nullable NtpBackgroundDataBase incomingTheme) {
        Map<String, Object> prefs = new HashMap<>();

        PrefService localStatePrefs = LocalStatePrefs.get();
        if (localStatePrefs != null) {
            String omniboxPositionPref = Pref.IS_OMNIBOX_IN_BOTTOM_POSITION;
            prefs.put(omniboxPositionPref, localStatePrefs.getBoolean(omniboxPositionPref));
        }

        if (UserPrefs.areNativePrefsLoaded(profile)) {
            PrefService userPrefs = UserPrefs.get(profile);
            if (userPrefs != null) {
                String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
                prefs.put(allCardsPref, userPrefs.getBoolean(allCardsPref));
                for (String key : MODULE_TYPE_TO_USER_PREFS_KEY.values()) {
                    prefs.put(key, userPrefs.getBoolean(key));
                }
            }
        }

        // Snapshot current NTP background theme so undo can restore the user's exact prior state.
        return new SyncedSetupSettings(prefs, getCurrentLocalTheme(incomingTheme));
    }

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

        @Nullable NtpBackgroundDataBase candidateTheme = settings.getTheme();
        boolean themeChange =
                candidateTheme != null
                        && candidateTheme.getPlatformType() != PlatformType.ANDROID
                        && importedSettingHasThemeChange(
                                candidateTheme, getCurrentLocalTheme(candidateTheme));

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
     * Checks whether preference values differ from local settings in a way that affects non-NTP
     * pages. Note that this method only checks preference values, not theme settings (even though
     * themes can also affect non-NTP pages via omnibox coloring). See {@link #shouldShowSnackbar}
     * where both preference and theme changes are checked.
     *
     * @param preferences The preferences to check.
     * @return whether the user's preferences differ from {@code preferences} in a way that affects
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
        // A null primary color means "unknown" (e.g. iOS never sends one), so the same image with a
        // null color on either side is not a theme change.
        return !candidateTheme.hasSameThemeAndCompatibleColor(currentTheme);
    }

    /**
     * Applies the given {@param settingsToApply}.
     *
     * @param profile The {@link Profile}.
     * @param settingsToApply The settings to apply.
     * @param nonNtp Whether only settings that affect non-NTP pages should be applied.
     */
    private void applySettings(
            Profile profile, SyncedSetupSettings settingsToApply, boolean nonNtp) {
        Log.i(
                TAG,
                "applySettings: prefs=%s, theme=%s, nonNtp=%s",
                settingsToApply.getPrefs().keySet(),
                settingsToApply.getTheme(),
                nonNtp);
        if (!nonNtp) {
            applyUserPrefSettings(profile, settingsToApply.getPrefs());
        }
        applyLocalStateSettings(settingsToApply.getPrefs());
        if (settingsToApply.getTheme() != null) {
            applyThemeSettings(settingsToApply.getTheme());
        }
    }

    /**
     * Applies {@param themeToApply} to the NTP customization manager and persists selection. If
     * {@param themeToApply} is null, clears custom background data back to the default NTP theme.
     */
    @VisibleForTesting
    void applyThemeSettings(@Nullable NtpBackgroundDataBase themeToApply) {
        Log.i(
                TAG,
                "applyThemeSettings: themeToApply=%s, isThemeImportSnackbarEnabled=%s",
                themeToApply,
                isThemeImportSnackbarEnabled());
        if (!isThemeImportSnackbarEnabled()) return;

        ensureNtpCustomizationConfigManagerInitialized();
        NtpCustomizationConfigManager configManager = NtpCustomizationConfigManager.getInstance();
        if (themeToApply instanceof NtpBackgroundDataImageBase imageBase
                && imageBase.getBitmap() == null) {
            // If the bitmap is null (e.g. from CrossDeviceThemeTracker before downloading), do not
            // write null to configManager. That would clobber the NTP background with null and
            // break rendering. NtpSyncedThemeManager handles the asynchronous download and
            // application of the image.
            if (themeToApply.getPlatformType() == PlatformType.ANDROID) {
                // On non-NTP pages, NewTabPage is not active to apply synced background
                // updates. Trigger applying the cached synced theme and recreating the
                // Activity.
                configManager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
            }
            return;
        }

        configManager.onBackgroundDataChanged(mContext, themeToApply);
        // Persist the user's selected background type so the imported theme survives app restarts.
        if (themeToApply != null) {
            configManager.maybeSaveUserSelectedBackgroundTypeToSharedPreference(mContext);
        }
        notifyApplyThemeChanges();
    }

    private void notifyApplyThemeChanges() {
        NtpThemeStateProvider.getInstance().notifyApplyThemeChanges();
    }

    /**
     * Applies the user pref settings from {@code preferencesToApply}.
     *
     * @param profile The {@link Profile}.
     * @param preferencesToApply The preferences to apply.
     */
    private void applyUserPrefSettings(Profile profile, Map<String, Object> preferencesToApply) {
        if (!UserPrefs.areNativePrefsLoaded(profile)) return;

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
            Log.i(TAG, "getPrefsFromRemoteDevice, res = %s", res);
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
        @Nullable DeviceOsAndFormFactor bestMatch =
                SyncedSetUpUtilsBridge.getBestMatchDeviceOsTypeAndFormFactor(tracker, profile);
        return themeTracker.getThemeForOsTypeAndFormFactor(
                mContext,
                bestMatch == null ? null : bestMatch.osType,
                bestMatch == null ? null : bestMatch.formFactor);
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

    /**
     * Updates pending snackbar state when either:
     *
     * <ol>
     *   <li>The active {@link Snackbar} is dismissed without its action button being clicked
     *       ({@link SnackbarManager.SnackbarController#onDismissNoAction}), or
     *   <li>This {@link CrossDeviceSettingImporter} is destroyed ({@link #destroy()}).
     * </ol>
     *
     * <p>Why both events share this handler: during an {@link Activity} termination or recreation,
     * {@link SnackbarManager} (which dismisses active snackbars during {@code Activity#onStop}) and
     * {@link ActivityLifecycleDispatcher} (which calls {@link #destroy()} on this importer during
     * {@code Activity#onDestroy}) may fire in either order—or {@link #destroy()} may run while a
     * snackbar is still queued behind a modal dialog before {@link SnackbarManager} ever shows it.
     *
     * <ul>
     *   <li>If the {@link Activity} is terminating for a configuration change or {@link
     *       Activity#recreate()} ({@link #isActivityTerminatingForConfigurationChange()}),
     *       whichever caller runs first promotes {@link #mActivePendingSnackbar} to static {@link
     *       #sPendingSnackbar} and clears {@link #mActivePendingSnackbar}. The second caller sees
     *       {@code mActivePendingSnackbar == null} and is a safe no-op.
     *   <li>If the {@link Activity} is <em>not</em> terminating for a configuration change (normal
     *       snackbar timeout dismissal or normal non-recreate {@link Activity} destruction), both
     *       {@link #mActivePendingSnackbar} and any task-matching {@link #sPendingSnackbar} are
     *       cleared.
     * </ul>
     */
    private void handleSnackbarDismissOrImporterDestroy() {
        if (isActivityTerminatingForConfigurationChange()) {
            if (mActivePendingSnackbar != null) {
                setPendingSnackbar(mActivePendingSnackbar);
            }
        } else if (sPendingSnackbar != null && matchesCurrentTask(sPendingSnackbar.taskId)) {
            setPendingSnackbar(null);
        }
        mActivePendingSnackbar = null;
    }

    /** Destroys this {@link CrossDeviceSettingImporter} and cleans up its observers. */
    public void destroy() {
        mIsDestroyed = true;
        mIsCrossOsThemeFetchInFlight = false;
        handleSnackbarDismissOrImporterDestroy();
        mActivityLifecycleDispatcher.unregister(this);
        mActivityTabSupplier.removeObserver(mTabChangeCallback);
        if (mObservedTab != null) {
            mObservedTab.removeObserver(mTabObserver);
            mObservedTab = null;
        }
        stopObservingLocalState();
        stopObservingPrefTracker();
        stopObservingThemeTracker();
        stopObservingModalDialogManager();
    }
}
