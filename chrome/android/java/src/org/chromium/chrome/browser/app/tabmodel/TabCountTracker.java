// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;

/**
 * Responsible for persisting the number of tabs in a tab model. Used to quickly determine how many
 * tabs need to be restored during startup without interacting with database task runner sequencing.
 *
 * <p>Counts are partitioned by {@code orchestratorType}, {@code windowTag}, and incognito (OTR)
 * status.
 */
@NullMarked
public class TabCountTracker {
    private static final String FILE_NAME = "tab_count_tracker";
    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";

    private static final String TAG_TABBED = "Tabbed";
    private static final String TAG_CUSTOM = "Custom";
    private static final String TAG_ARCHIVED = "Archived";
    private static final String TAG_HEADLESS = "Headless";

    private final String mWindowTag;
    private final String mRegularModelNumTabsKey;
    private final String mIncognitoModelNumTabsKey;

    /**
     * @param windowTag The tag for the window being tracked.
     * @param orchestratorType The orchestrator type representing the tab model orchestrator.
     */
    public TabCountTracker(String windowTag, @TabOrchestratorType int orchestratorType) {
        mWindowTag = windowTag;
        mRegularModelNumTabsKey =
                getNumTabsKeyForModel(windowTag, orchestratorType, /* incognito= */ false);
        mIncognitoModelNumTabsKey =
                getNumTabsKeyForModel(windowTag, orchestratorType, /* incognito= */ true);
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
     * Clears all tab count preferences associated with the current window across all orchestrator
     * types. Called when a window is closed or destroyed.
     */
    public void clearCurrentWindow() {
        cleanupWindow(mWindowTag);
    }

    /**
     * Cleans up all tab count preferences associated with a specific window across all orchestrator
     * types. Typically called when a window is closed or destroyed.
     *
     * @param windowTag The tag for the window to clean up.
     */
    public static void cleanupWindow(String windowTag) {
        SharedPreferences.Editor editor = getSharedPreferences().edit();
        for (@TabOrchestratorType int type = 0; type < TabOrchestratorType.NUM_ENTRIES; type++) {
            editor.remove(getNumTabsKeyForModel(windowTag, type, /* incognito= */ false));
            editor.remove(getNumTabsKeyForModel(windowTag, type, /* incognito= */ true));
        }
        editor.apply();
    }

    /** Clears all tab count data for all windows. */
    public static void clearGlobalState() {
        getSharedPreferences().edit().clear().apply();
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(FILE_NAME, Context.MODE_PRIVATE);
    }

    private static String getNumTabsKeyForModel(
            String windowTag, @TabOrchestratorType int orchestratorType, boolean incognito) {
        String orchestratorTag = getOrchestratorTag(orchestratorType);
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        return orchestratorTag + "_" + windowTag + suffix;
    }

    private static String getOrchestratorTag(@TabOrchestratorType int orchestratorType) {
        switch (orchestratorType) {
            case TabOrchestratorType.TABBED:
                return TAG_TABBED;
            case TabOrchestratorType.CUSTOM:
                return TAG_CUSTOM;
            case TabOrchestratorType.ARCHIVED:
                return TAG_ARCHIVED;
            case TabOrchestratorType.HEADLESS:
                return TAG_HEADLESS;
            default:
                throw new IllegalStateException("Unknown orchestrator type: " + orchestratorType);
        }
    }
}
