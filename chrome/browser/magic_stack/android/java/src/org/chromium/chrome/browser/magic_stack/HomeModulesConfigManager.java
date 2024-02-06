// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

import java.util.Set;

public interface HomeModulesConfigManager {
    /** An interface to use for getting home modules related updates. */
    interface HomeModulesStateListener {
        /** Called when the home modules' specific module type is disabled or enabled. */
        void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled);
    }

    /**
     * Adds a {@link HomeModulesStateListener} to receive updates when the home modules state
     * changes.
     */
    void addListener(HomeModulesStateListener listener);

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    void removeListener(HomeModulesStateListener listener);

    /**
     * Returns the set which contains all the module types that are registered and enabled according
     * to user preference.
     */
    @ModuleType
    Set<Integer> getEnabledModuleList();

    /**
     * Sets the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled);
}
