// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import static org.chromium.chrome.browser.ntp_customization.ntp_cards.NtpCardsMediator.MODULE_TYPE_TO_USER_PREFS_KEY;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.TYPE_ACTION;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_IMPORT;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_REDO;
import static org.chromium.chrome.browser.ui.messages.snackbar.Snackbar.UMA_CROSS_DEVICE_SETTING_UNDO;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

@NullMarked
public class CrossDeviceSettingImporter implements TopResumedActivityChangedObserver {

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final NullableObservableSupplier<Tab> mActivityTabSupplier;
    private final Context mContext;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Callback<@Nullable Tab> mTabChangeCallback =
            new Callback<@Nullable Tab>() {
                @Override
                public void onResult(@Nullable Tab tab) {
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
        mActivityTabSupplier.addObserver(mTabChangeCallback);
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
    private void onTabChangeOrGainFocus(@Nullable Tab currentTab) {
        if (currentTab == null) return;
        // TODO(crbug.com/475543024): Implement.
    }

    /**
     * Shows {@param snackbar} now if there no dialogs, or waits until the last dialog is dismissed
     * and then shows it.
     */
    @VisibleForTesting
    public void showSnackbarAfterDialogs(Snackbar snackbar) {
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
                        }
                    });
        } else {
            snackbarManager.showSnackbar(snackbar);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to import NTP settings from another device.
     *
     * @param preferencesToApply The preferences that will be applied.
     */
    @VisibleForTesting
    void askToApplyNtpSettingImportIfNeeded(Map<String, Object> preferencesToApply) {
        if (importedSettingsHavePreferenceChange(preferencesToApply)) {
            Snackbar offerApplySnackbar =
                    Snackbar.make(
                            mContext.getString(R.string.synced_set_up_snackbar_ask_to_apply),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    applyAndNotifyNtpSettingImport(preferencesToApply);
                                    RecordUserAction.record(
                                            "Android.CrossDeviceSettingImport.Apply");
                                }
                            },
                            TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_IMPORT);
            offerApplySnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.apply),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerApplySnackbar);
        }
    }

    /**
     * Applies settings from another device and shows a snackbar to the user, informing them that
     * their settings were applied and offering an undo button.
     *
     * @param preferencesToApply The preferences that will be applied.
     */
    private void applyAndNotifyNtpSettingImport(Map<String, Object> preferencesToApply) {
        if (importedSettingsHavePreferenceChange(preferencesToApply)) {
            Map<String, Object> currentPreferences = getCurrentSettings();
            Snackbar offerUndoSnackbar =
                    Snackbar.make(
                            mContext.getString(
                                    R.string.synced_set_up_snackbar_applied_confirmation),
                            new SnackbarManager.SnackbarController() {
                                @Override
                                public void onAction(@Nullable Object actionData) {
                                    applySettings(currentPreferences);
                                    askToRedoNtpSettingImport(preferencesToApply);
                                    RecordUserAction.record(
                                            "Android.CrossDeviceSettingImport.Undo");
                                }
                            },
                            Snackbar.TYPE_ACTION,
                            UMA_CROSS_DEVICE_SETTING_UNDO);
            offerUndoSnackbar.setAction(
                    /* actionText= */ mContext.getString(R.string.undo),
                    /* actionData= */ Map.of());
            showSnackbarAfterDialogs(offerUndoSnackbar);
            applySettings(preferencesToApply);
        }
    }

    /**
     * Shows a snackbar asking the user if they want to redo their NTP setting import (this is
     * offered after the user hits undo).
     *
     * @param preferencesToApply The preferences that will be applied during the redo.
     */
    private void askToRedoNtpSettingImport(Map<String, Object> preferencesToApply) {
        Snackbar offerRedoSnackbar =
                Snackbar.make(
                        mContext.getString(R.string.synced_set_up_snackbar_removed_confirmation),
                        new SnackbarManager.SnackbarController() {
                            @Override
                            public void onAction(@Nullable Object actionData) {
                                applyAndNotifyNtpSettingImport(preferencesToApply);
                                RecordUserAction.record("Android.CrossDeviceSettingImport.Redo");
                            }
                        },
                        TYPE_ACTION,
                        UMA_CROSS_DEVICE_SETTING_REDO);
        offerRedoSnackbar.setAction(
                /* actionText= */ mContext.getString(R.string.redo), /* actionData= */ Map.of());
        showSnackbarAfterDialogs(offerRedoSnackbar);
    }

    /** Returns the user's current settings. */
    private Map<String, Object> getCurrentSettings() {
        // Assumes mTab.getProfile() is non-null and UserPrefs.areNativePrefsLoaded is true.
        Profile profile = mActivityTabSupplier.get().getProfile();
        PrefService userPrefs = UserPrefs.get(profile);
        if (userPrefs == null) return Map.of();

        Map<String, Object> result = new HashMap<>();
        String allCardsPref = Pref.MAGIC_STACK_HOME_MODULE_ENABLED;
        result.put(allCardsPref, userPrefs.getBoolean(allCardsPref));
        for (String key : MODULE_TYPE_TO_USER_PREFS_KEY.values()) {
            result.put(key, userPrefs.getBoolean(key));
        }

        return result;
    }

    /**
     * @param preferences The preferences to check.
     * @return whether the user's current settings are different from {@param preferences}.
     */
    private boolean importedSettingsHavePreferenceChange(Map<String, Object> preferences) {
        // Assumes mTab.getProfile() is non-null and UserPrefs.areNativePrefsLoaded is true.
        Profile profile = mActivityTabSupplier.get().getProfile();
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
        Profile profile = mActivityTabSupplier.get().getProfile();
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
    }

    /** Destroys the {@link CrossDeviceSettingImporter}. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityTabSupplier.removeObserver(mTabChangeCallback);
    }
}
