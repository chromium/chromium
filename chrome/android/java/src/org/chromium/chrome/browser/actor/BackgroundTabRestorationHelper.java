// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;

import java.util.Collections;
import java.util.Set;

/**
 * Shared helper utilities for intercepting and restoring tabs managed by {@link BackgroundTabPool}.
 */
@NullMarked
public final class BackgroundTabRestorationHelper {
    private BackgroundTabRestorationHelper() {}

    /**
     * Returns whether background tab interception should occur.
     *
     * @param orchestratorType The orchestrator type for the tab store or restorer.
     * @param isIncognito Whether the model is off-the-record.
     * @return Whether background tabs should be intercepted.
     */
    public static boolean shouldIntercept(
            @TabOrchestratorType int orchestratorType, boolean isIncognito) {
        return orchestratorType == TabOrchestratorType.TABBED
                && ActorUtils.isBackgroundActuationEnabled()
                && !isIncognito;
    }

    /**
     * Acquires a leased {@link BackgroundTabPool} instance for the regular profile of the given
     * selector.
     *
     * @param selector The {@link TabModelSelector} to query for profile.
     * @return The leased {@link BackgroundTabPool}, or null if unavailable or incognito.
     */
    public static @Nullable BackgroundTabPool acquirePool(@Nullable TabModelSelector selector) {
        assertOnUiThread();
        if (selector == null) return null;

        TabModel model = selector.getModel(/* incognito= */ false);
        if (model == null) return null;

        Profile profile = model.getProfile();
        if (profile == null || profile.isOffTheRecord()) return null;

        if (!BackgroundTabPoolManager.hasPoolForTesting() && !profile.isNativeInitialized()) {
            return null;
        }

        return BackgroundTabPoolManager.acquire(profile);
    }

    /**
     * Fetches the set of background tab IDs currently held or cached in {@link BackgroundTabPool}.
     *
     * @param orchestratorType The orchestrator type for the caller.
     * @param selector The {@link TabModelSelector} to acquire pool from.
     * @param isIncognito Whether the caller is incognito.
     * @return A {@link Set} of {@link TabId} integers.
     */
    public static Set<@TabId Integer> fetchBackgroundTabIds(
            @TabOrchestratorType int orchestratorType,
            @Nullable TabModelSelector selector,
            boolean isIncognito) {
        assertOnUiThread();
        if (!shouldIntercept(orchestratorType, isIncognito)) {
            return Collections.emptySet();
        }

        BackgroundTabPool pool = acquirePool(selector);
        if (pool == null) return Collections.emptySet();

        try {
            return pool.getAllTabIds();
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }

    /**
     * Attempts to restore and attach a background tab from {@link BackgroundTabPool}.
     *
     * @param orchestratorType The orchestrator type for the caller.
     * @param selector The {@link TabModelSelector} managing tab models.
     * @param tabId The ID of the background tab to restore.
     * @param index The index to insert the restored tab into the model.
     * @param tabState Optional placeholder {@link TabState} whose WebContentsState will be
     *     destroyed upon attachment.
     * @return The restored {@link Tab}, or null if not found or restoration failed.
     */
    public static @Nullable Tab maybeRestoreBackgroundTab(
            @TabOrchestratorType int orchestratorType,
            @Nullable TabModelSelector selector,
            @TabId int tabId,
            int index,
            @Nullable TabState tabState) {
        assertOnUiThread();
        if (!shouldIntercept(orchestratorType, /* isIncognito= */ false) || selector == null) {
            return null;
        }

        BackgroundTabPool pool = acquirePool(selector);
        if (pool == null) return null;

        try {
            BackgroundPoolTab backgroundTab = pool.loadTab(tabId, tabId);
            if (backgroundTab == null) return null;

            if (tabState != null && tabState.contentsState != null) {
                tabState.contentsState.destroy();
            }
            TabModel model = selector.getModel(/* incognito= */ false);
            return backgroundTab.attachTab(model, index);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }
}
