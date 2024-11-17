// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.content.Context;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Provides information regarding chrome home modules enabled states.
 *
 * <p>This class serves as a single chrome home modules setting logic gateway.
 */
public class HomeModulesConfigManager {
    /** An interface to use for getting home modules related updates. */
    interface HomeModulesStateListener {
        /** Called when the home modules' specific module type is disabled or enabled. */
        void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled);
    }

    static final long INVALID_TIMESTAMP = -1;
    static final int INVALID_FRESHNESS_SCORE = -1;

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomeModulesStateListener> mHomepageStateListeners;

    /** A map of <ModuleType, ModuleEligibilityChecker>. */
    private final Map<Integer, ModuleConfigChecker> mModuleConfigCheckerMap;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static HomeModulesConfigManager sInstance = new HomeModulesConfigManager();
    }

    /** Returns the singleton instance of HomeModulesConfigManager. */
    public static HomeModulesConfigManager getInstance() {
        return LazyHolder.sInstance;
    }

    private HomeModulesConfigManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mHomepageStateListeners = new ObserverList<>();
        mModuleConfigCheckerMap = new HashMap<>();
    }

    public void registerModuleEligibilityChecker(
            @ModuleType int moduleType, ModuleConfigChecker eligibilityChecker) {
        mModuleConfigCheckerMap.put(moduleType, eligibilityChecker);
    }

    /**
     * Adds a {@link HomeModulesStateListener} to receive updates when the home modules state
     * changes.
     */
    void addListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    void removeListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Menu click handler on customize button.
     *
     * @param context {@link Context} used for launching a settings activity.
     */
    public void onMenuClick(Context context) {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, HomeModulesConfigSettings.class);
    }

    /**
     * Notifies any listeners about a chrome home modules state change.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    private void notifyModuleTypeUpdated(@ModuleType int moduleType, boolean enabled) {
        for (HomeModulesStateListener listener : mHomepageStateListeners) {
            listener.onModuleConfigChanged(moduleType, enabled);
        }
    }

    /**
     * Returns the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     */
    public boolean getPrefModuleTypeEnabled(@ModuleType int moduleType) {
        return mSharedPreferencesManager.readBoolean(getSettingsPreferenceKey(moduleType), true);
    }

    /**
     * Sets the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled) {
        mSharedPreferencesManager.writeBoolean(getSettingsPreferenceKey(moduleType), enabled);
        notifyModuleTypeUpdated(moduleType, enabled);
    }

    /**
     * Returns the set which contains all the module types that are registered and enabled according
     * to user preference. Note: this function should be called after profile is ready.
     */
    @ModuleType
    public Set<Integer> getEnabledModuleSet() {
        @ModuleType Set<Integer> enabledModuleList = new HashSet<>();
        for (Entry<Integer, ModuleConfigChecker> entry : mModuleConfigCheckerMap.entrySet()) {
            ModuleConfigChecker configChecker = entry.getValue();
            if (configChecker.isEligible() && getPrefModuleTypeEnabled(entry.getKey())) {
                enabledModuleList.add(entry.getKey());
            }
        }
        return enabledModuleList;
    }

    /** Returns a list of modules that allow users to configure in settings. */
    @ModuleType
    public List<Integer> getModuleListShownInSettings() {
        @ModuleType List<Integer> moduleListShownInSettings = new ArrayList<>();
        for (Entry<Integer, ModuleConfigChecker> entry : mModuleConfigCheckerMap.entrySet()) {
            ModuleConfigChecker configChecker = entry.getValue();
            if (configChecker.isEligible()) {
                moduleListShownInSettings.add(entry.getKey());
            }
        }
        return moduleListShownInSettings;
    }

    /** Returns whether it has any module to configure in settings. */
    public boolean hasModuleShownInSettings() {
        for (ModuleConfigChecker moduleConfigChecker : mModuleConfigCheckerMap.values()) {
            if (moduleConfigChecker.isEligible()) {
                return true;
            }
        }
        return false;
    }

    /** Returns the preference key of the module type. */
    String getSettingsPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        // SINGLE_TAB and TAB_RESUMPTION modules are controlled by the same preference key.
        if (moduleType == ModuleType.SINGLE_TAB) {
            return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                    String.valueOf(ModuleType.TAB_RESUMPTION));
        }
        return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(moduleType));
    }

    /** Returns the preference key of the module type. */
    String getFreshnessCountPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                String.valueOf(moduleType));
    }

    /** Gets the freshness count of a module. */
    public int getFreshnessCount(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        return sharedPreferencesManager.readInt(freshnessScoreKey, INVALID_FRESHNESS_SCORE);
    }

    /** Called to reset the freshness count when there is new information to show. */
    public void resetFreshnessCount(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, INVALID_FRESHNESS_SCORE);
    }

    /**
     * Called to increase the freshness score for the module. The count is increased from 0, not -1.
     */
    public void increaseFreshnessCount(@ModuleType int moduleType, int count) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        int score =
                Math.max(
                        0,
                        sharedPreferencesManager.readInt(
                                freshnessScoreKey, INVALID_FRESHNESS_SCORE));
        sharedPreferencesManager.writeInt(freshnessScoreKey, (score + count));
    }

    /** Returns the preference key of the module type. */
    String getFreshnessTimeStampPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_FRESHNESS_TIMESTAMP_MS.createKey(
                String.valueOf(moduleType));
    }

    /** Sets the timestamp of last time a freshness score is logged for a module. */
    public void setFreshnessScoreTimeStamp(@ModuleType int moduleType, long timeStampMs) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreTimeStampKey = getFreshnessTimeStampPreferenceKey(moduleType);
        sharedPreferencesManager.writeLong(freshnessScoreTimeStampKey, timeStampMs);
    }

    /** Gets the timestamp of last time a freshness score is logged for a module. */
    public long getFreshnessScoreTimeStamp(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreTimeStampKey = getFreshnessTimeStampPreferenceKey(moduleType);
        return sharedPreferencesManager.readLong(freshnessScoreTimeStampKey, INVALID_TIMESTAMP);
    }

    /** Sets a mocked instance for testing. */
    public static void setInstanceForTesting(HomeModulesConfigManager instance) {
        var oldValue = LazyHolder.sInstance;
        LazyHolder.sInstance = instance;
        ResettersForTesting.register(() -> LazyHolder.sInstance = oldValue);
    }

    public void setFreshnessCountForTesting(@ModuleType int moduleType, int count) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessCountPreferenceKey(moduleType);
        sharedPreferencesManager.writeInt(freshnessScoreKey, count);
    }

    public void resetFreshnessTimeStampForTesting(@ModuleType int moduleType) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        String freshnessScoreKey = getFreshnessTimeStampPreferenceKey(moduleType);
        sharedPreferencesManager.removeKey(freshnessScoreKey);
    }

    public void cleanupForTesting() {
        mModuleConfigCheckerMap.clear();
    }
}
