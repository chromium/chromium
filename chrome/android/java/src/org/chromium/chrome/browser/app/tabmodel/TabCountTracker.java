// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Responsible for persisting the number of tabs in a tab model. Used to quickly determine how many
 * tabs need to be restored during startup without interacting with database task runner sequencing.
 */
@NullMarked
public class TabCountTracker {
    private static final String FILE_NAME = "tab_count_tracker";
    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";

    private final String mRegularModelNumTabsKey;
    private final String mIncognitoModelNumTabsKey;

    /**
     * @param windowTag The tag for the window being tracked.
     */
    public TabCountTracker(String windowTag) {
        mRegularModelNumTabsKey = getNumTabsKeyForModel(windowTag, /* incognito= */ false);
        mIncognitoModelNumTabsKey = getNumTabsKeyForModel(windowTag, /* incognito= */ true);
    }

    /**
     * Retrieves the persisted tab count for a specific model, or 0 if no record exists.
     *
     * @param incognito Whether to get the count for the incognito model.
     */
    public int getRestoredTabCount(boolean incognito) {
        return getSharedPreferences()
                .getInt(incognito ? mIncognitoModelNumTabsKey : mRegularModelNumTabsKey, 0);
    }

    /**
     * Updates the persisted tab count for a specific model.
     *
     * @param incognito Whether to update the incognito model count.
     * @param count The current number of tabs in the model.
     */
    public void updateTabCount(boolean incognito, int count) {
        getSharedPreferences()
                .edit()
                .putInt(incognito ? mIncognitoModelNumTabsKey : mRegularModelNumTabsKey, count)
                .apply();
    }

    /**
     * Removes the persisted tab count for a specific model.
     *
     * @param incognito Whether to clear the incognito model count.
     */
    public void clearTabCount(boolean incognito) {
        getSharedPreferences()
                .edit()
                .remove(incognito ? mIncognitoModelNumTabsKey : mRegularModelNumTabsKey)
                .apply();
    }

    /**
     * Clears all tab count preferences associated with the current window. Called when a window is
     * closed or destroyed.
     */
    public void clearCurrentWindow() {
        getSharedPreferences()
                .edit()
                .remove(mRegularModelNumTabsKey)
                .remove(mIncognitoModelNumTabsKey)
                .apply();
    }

    /**
     * Cleans up all tab count preferences associated with a specific window. Typically called when
     * a window is closed or destroyed.
     *
     * @param windowTag The tag for the window to clean up.
     */
    public static void cleanupWindow(String windowTag) {
        String regularKey = getNumTabsKeyForModel(windowTag, /* incognito= */ false);
        String incognitoKey = getNumTabsKeyForModel(windowTag, /* incognito= */ true);

        getSharedPreferences().edit().remove(regularKey).remove(incognitoKey).apply();
    }

    /** Clears all tab count data for all windows. */
    public static void clearGlobalState() {
        getSharedPreferences().edit().clear().apply();
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(FILE_NAME, Context.MODE_PRIVATE);
    }

    private static String getNumTabsKeyForModel(String windowTag, boolean incognito) {
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        return windowTag + suffix;
    }
}
