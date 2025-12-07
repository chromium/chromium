// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Handles the sharing Tabs with the system state changes in settings. */
@NullMarked
public class AuxiliarySearchConfigManager {
    /** An interface to get updates of the sharing Tabs with the system state. */
    public interface ShareTabsWithOsStateListener {
        /** Called when the config of whether to share Tabs with the system is changed. */
        void onConfigChanged(boolean isEnabled);
    }

    private final ObserverList<ShareTabsWithOsStateListener> mShareTabsWithOsStateListeners;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final AuxiliarySearchConfigManager sInstance = new AuxiliarySearchConfigManager();
    }

    /** Returns the singleton instance of AuxiliarySearchConfigManager. */
    public static AuxiliarySearchConfigManager getInstance() {
        return LazyHolder.sInstance;
    }

    private AuxiliarySearchConfigManager() {
        mShareTabsWithOsStateListeners = new ObserverList<>();
    }

    /**
     * Adds a {@link ShareTabsWithOsStateListener} to receive updates when the sharing Tabs state
     * changes.
     *
     * @param listener The listener to add.
     */
    @VisibleForTesting
    public void addListener(ShareTabsWithOsStateListener listener) {
        mShareTabsWithOsStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    @VisibleForTesting
    public void removeListener(ShareTabsWithOsStateListener listener) {
        mShareTabsWithOsStateListeners.removeObserver(listener);
    }

    /**
     * Notifies any listeners about the sharing Tabs with the system state change.
     *
     * @param enabled True is the sharing Tabs with the system is enabled.
     */
    public void notifyShareTabsStateChanged(boolean enabled) {
        // Saves the user's response here in case the user changed the setting without clicking the
        // opt-in card on the magic stack.
        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        prefManager.writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, true);

        for (ShareTabsWithOsStateListener listener : mShareTabsWithOsStateListeners) {
            listener.onConfigChanged(enabled);
        }
    }

    public int getObserverListSizeForTesting() {
        return mShareTabsWithOsStateListeners.size();
    }
}
