// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getSettingsPreferenceKey;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Provides information regarding chrome home modules enabled states.
 *
 * <p>This class serves as a single chrome home modules setting logic gateway.
 */
@NullMarked
public class HomeModulesConfigManager {
    /** An interface to use for getting home modules related updates. */
    public interface HomeModulesStateListener {
        /** Called when the home modules' specific module type is disabled or enabled. */
        default void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled) {}

        /** Called when the "all cards" switch is disabled or enabled. */
        default void allCardsConfigChanged(boolean isEnabled) {}
    }

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomeModulesStateListener> mHomepageStateListeners;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static HomeModulesConfigManager sInstance = new HomeModulesConfigManager();
    }

    /** Returns the singleton instance of HomeModulesConfigManager. */
    public static HomeModulesConfigManager getInstance() {
        return LazyHolder.sInstance;
    }

    @VisibleForTesting
    HomeModulesConfigManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mHomepageStateListeners = new ObserverList<>();
    }

    /**
     * Adds a {@link HomeModulesStateListener} to receive updates when the home modules state
     * changes.
     */
    public void addListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    public void removeListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
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
    public void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled) {
        mSharedPreferencesManager.writeBoolean(getSettingsPreferenceKey(moduleType), enabled);
        notifyModuleTypeUpdated(moduleType, enabled);
    }

    /** Returns the user preference for whether all cards in the magic stack are enabled. */
    public boolean getPrefAllCardsEnabled() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, true);
    }

    /**
     * Sets the user preference for whether all cards in the magic stack are enabled.
     *
     * @param enabled True is all cards are enabled.
     */
    public void setPrefAllCardsEnabled(boolean enabled) {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, enabled);
        for (HomeModulesStateListener listener : mHomepageStateListeners) {
            listener.allCardsConfigChanged(enabled);
        }
    }

    /** Sets a mocked instance for testing. */
    public static void setInstanceForTesting(HomeModulesConfigManager instance) {
        var oldValue = LazyHolder.sInstance;
        LazyHolder.sInstance = instance;
        ResettersForTesting.register(() -> LazyHolder.sInstance = oldValue);
    }
}
