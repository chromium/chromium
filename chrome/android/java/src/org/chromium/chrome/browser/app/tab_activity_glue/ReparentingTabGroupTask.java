// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.List;

/** Handles the setup of the Intent to move an entire tab group to a different activity. */
@NullMarked
public class ReparentingTabGroupTask {
    private TabGroupMetadata mTabGroupMetadata;

    /**
     * @param tabGroupMetadata {@link TabGroupMetadata} object contains the tab group properties.
     * @return {@link ReparentingTabGroupTask} object initialized with the specified tab group
     *     metadata and grouped tabs.
     */
    public static ReparentingTabGroupTask from(TabGroupMetadata tabGroupMetadata) {
        return new ReparentingTabGroupTask(tabGroupMetadata);
    }

    private ReparentingTabGroupTask(TabGroupMetadata tabGroupMetadata) {
        mTabGroupMetadata = tabGroupMetadata;
    }

    /**
     * Sets up the given intent to be used for re-parenting an entire tab group.
     *
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *     launching the new host activity. This intent's URI and action will be overridden. This
     *     may be null if no intent customization is needed.
     * @param finalizeCallback A callback that will be called after the tab group is attached to the
     *     new host activity in {@link #attachAndFinishReparenting}.
     */
    public void setupIntent(Intent intent, @Nullable Runnable finalizeCallback) {
        // 1. Create a new Intent if none is provided.
        if (intent == null) intent = new Intent();

        // 2. Ensure intent targets the correct activity and action.
        if (intent.getComponent() == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);

        // 3. Setup `TabReparentingParams` and detach for each tab.
        @Nullable List<Tab> groupedTabs =
                TabWindowManagerSingleton.getInstance()
                        .getGroupedTabsByWindow(
                                mTabGroupMetadata.sourceWindowId,
                                mTabGroupMetadata.rootId,
                                mTabGroupMetadata.isIncognito);
        if (groupedTabs == null || groupedTabs.size() == 0) return;
        for (Tab tab : groupedTabs) {
            AsyncTabParamsManagerSingleton.getInstance()
                    .add(tab.getId(), new TabReparentingParams(tab, finalizeCallback));
            ReparentingTask.from(tab).detach();
        }

        // 4. Store tab group metadata into intent and add trusted intent extras.
        IntentHandler.setTabGroupMetadata(intent, mTabGroupMetadata);
        IntentUtils.addTrustedIntentExtras(intent);
    }
}
