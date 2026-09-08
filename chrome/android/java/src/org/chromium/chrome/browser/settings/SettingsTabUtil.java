// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

/** Utility methods for settings tabs. */
@NullMarked
public class SettingsTabUtil {
    private SettingsTabUtil() {}

    /** Returns whether the given tab is an open settings tab. */
    public static boolean isSettingsTab(@Nullable Tab tab) {
        if (tab == null || tab.isClosing() || tab.isDestroyed() || tab.isIncognito()) return false;

        return tab.getNativePage() instanceof SettingsPage;
    }

    /**
     * Finds an open settings tab in the given {@link TabModelSelector}'s regular tab model.
     *
     * @param tabModelSelector The tab model selector to search.
     * @return The open settings tab, or null if none is open.
     */
    public static @Nullable Tab findSettingsTab(@Nullable TabModelSelector tabModelSelector) {
        if (tabModelSelector == null) return null;

        TabModel regularModel = tabModelSelector.getModel(/* incognito= */ false);
        if (regularModel == null) return null;
        for (int i = 0; i < regularModel.getCount(); i++) {
            Tab tab = regularModel.getTabAt(i);
            if (isSettingsTab(tab)) {
                return tab;
            }
        }
        return null;
    }

    /**
     * Activates an existing settings tab in the given {@link TabModelSelector}.
     *
     * @param tabModelSelector The tab model selector containing the tab.
     * @param tab The settings tab to activate.
     */
    public static void activateSettingsTab(TabModelSelector tabModelSelector, Tab tab) {
        assert tab.getNativePage() instanceof SettingsPage;

        // Settings tabs only exist in the regular (non-incognito) tab model.
        // Switch to the regular model before selecting the tab.
        if (tabModelSelector.isIncognitoSelected()) {
            tabModelSelector.selectModel(/* incognito= */ false);
        }
        TabModel regularModel = tabModelSelector.getModel(/* incognito= */ false);
        int index = regularModel.indexOf(tab);
        if (index != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(regularModel, index);
        }
    }
}
