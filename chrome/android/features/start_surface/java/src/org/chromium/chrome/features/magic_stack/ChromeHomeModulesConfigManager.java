// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.magic_stack;

import android.content.Context;

import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.HashSet;
import java.util.Set;

/**
 * Provides information regarding chrome home modules enabled states.
 *
 * <p>This class serves as a single chrome home modules setting logic gateway.
 */
public class ChromeHomeModulesConfigManager implements HomeModulesConfigManager {
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomeModulesStateListener> mChromeHomepageStateListeners;
    private final SettingsLauncher mSettingsLauncher;

    private static class LazyHolder {
        private static final ChromeHomeModulesConfigManager INSTANCE =
                new ChromeHomeModulesConfigManager();
    }

    /** Returns the singleton instance of ChromeHomeModulesConfigManager. */
    public static ChromeHomeModulesConfigManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    private ChromeHomeModulesConfigManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mChromeHomepageStateListeners = new ObserverList<>();
        mSettingsLauncher = new SettingsLauncherImpl();
    }

    @Override
    public void addListener(HomeModulesStateListener listener) {
        mChromeHomepageStateListeners.addObserver(listener);
    }

    @Override
    public void removeListener(HomeModulesStateListener listener) {
        mChromeHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Menu click handler on customize button.
     *
     * @param context {@link Context} used for launching a settings activity.
     */
    public void onMenuClick(Context context) {
        mSettingsLauncher.launchSettingsActivity(context, ChromeHomeModulesConfigSettings.class);
    }

    /**
     * Notifies any listeners about a chrome home modules state change.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    private void notifyModuleTypeUpdated(@ModuleType int moduleType, boolean enabled) {
        for (HomeModulesStateListener listener : mChromeHomepageStateListeners) {
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
    @Override
    public void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled) {
        mSharedPreferencesManager.writeBoolean(getPreferenceKey(moduleType), enabled);
        notifyModuleTypeUpdated(moduleType, enabled);
    }

    @Override
    public @ModuleType Set<Integer> getEnabledModuleList() {
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
        return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(moduleType));
    }
}
