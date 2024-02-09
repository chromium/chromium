// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import android.content.Context;

import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.HashSet;
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

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomeModulesStateListener> mHomepageStateListeners;

    private static final HomeModulesConfigManager sInstance = new HomeModulesConfigManager();

    /** Returns the singleton instance of HomeModulesConfigManager. */
    public static HomeModulesConfigManager getInstance() {
        return sInstance;
    }

    private HomeModulesConfigManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mHomepageStateListeners = new ObserverList<>();
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
     * @param settingsLauncher {@link SettingsLauncher} used for launching a settings activity.
     */
    public void onMenuClick(Context context, SettingsLauncher settingsLauncher) {
        settingsLauncher.launchSettingsActivity(context, HomeModulesConfigSettings.class);
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
    boolean getPrefModuleTypeEnabled(@ModuleType int moduleType) {
        return mSharedPreferencesManager.readBoolean(getPreferenceKey(moduleType), true);
    }

    /**
     * Sets the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled) {
        mSharedPreferencesManager.writeBoolean(getPreferenceKey(moduleType), enabled);
        notifyModuleTypeUpdated(moduleType, enabled);
    }

    /**
     * Returns the set which contains all the module types that are registered and enabled according
     * to user preference.
     */
    @ModuleType
    Set<Integer> getEnabledModuleList() {
        ModuleRegistry moduleRegistry = ModuleRegistry.getInstance();
        @ModuleType Set<Integer> moduleTypeRegistered = moduleRegistry.getRegisteredModuleTypes();
        @ModuleType Set<Integer> enabledModuleList = new HashSet<>();
        for (@ModuleType int moduleType : moduleTypeRegistered) {
            if (!moduleRegistry.isModuleConfigurable(moduleType)
                    || (moduleRegistry.isModuleEligibleToBuild(moduleType)
                            && getPrefModuleTypeEnabled(moduleType))) {
                enabledModuleList.add(moduleType);
            }
        }
        return enabledModuleList;
    }

    /** Returns the preference key of the module type. */
    String getPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(moduleType));
    }
}
