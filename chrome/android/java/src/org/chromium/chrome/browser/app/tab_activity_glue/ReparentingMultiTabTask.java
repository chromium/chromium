// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.ArrayList;
import java.util.List;

/** Takes care of reparenting a list of Tab objects from one Activity to another. */
@NullMarked
public class ReparentingMultiTabTask {
    private final List<Tab> mTabs;

    /**
     * Creates a new task to reparent a list of tabs.
     *
     * @param tabs The list of {@link Tab} objects to reparent.
     * @return A new {@link ReparentingMultiTabTask} object.
     */
    public static ReparentingMultiTabTask from(List<Tab> tabs) {
        return new ReparentingMultiTabTask(tabs);
    }

    private ReparentingMultiTabTask(List<Tab> tabs) {
        mTabs = tabs;
    }

    /**
     * Begins the tabs reparenting process. Detaches the tab from its current activity and fires an
     * Intent to reparent the tab into its new host activity.
     *
     * @param context {@link Context} object used to start a new activity.
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *     launching the new host activity. This intent's URI and action will be overridden. This
     *     may be null if no intent customization is needed.
     */
    public void begin(Context context, Intent intent) {
        setupIntent(intent, null);
        context.startActivity(intent, null);
    }

    /**
     * Sets up the given intent to be used for reparenting multiple tabs. This method detaches each
     * tab from its current Activity and stores its parameters in {@link
     * AsyncTabParamsManagerSingleton} to be claimed by the new Activity.
     *
     * @param intent An optional intent with the desired component, flags, or extras. A new one will
     *     be created if null. This intent's URI and action will be overridden.
     * @param finalizeCallback A callback that will be called for each tab after it is attached to
     *     the new host activity.
     */
    public void setupIntent(Intent intent, @Nullable Runnable finalizeCallback) {
        if (intent == null) intent = new Intent();
        if (intent.getComponent() == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);

        if (mTabs == null || mTabs.isEmpty()) return;

        ArrayList<Integer> tabIdsToReparent = new ArrayList<>();
        ArrayList<String> urlsToReparent = new ArrayList<>();
        boolean isIncognito = mTabs.get(0).isIncognito();

        for (Tab tab : mTabs) {
            tabIdsToReparent.add(tab.getId());
            urlsToReparent.add(tab.getUrl().getSpec());
            AsyncTabParamsManagerSingleton.getInstance()
                    .add(tab.getId(), new TabReparentingParams(tab, finalizeCallback));
            ReparentingTask.from(tab).detach();
        }

        if (isIncognito) {
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }

        Bundle multiTabBundle = new Bundle();
        multiTabBundle.putIntegerArrayList(IntentHandler.MULTI_TAB_KEY_TAB_IDS, tabIdsToReparent);
        multiTabBundle.putStringArrayList(IntentHandler.MULTI_TAB_KEY_TAB_URLS, urlsToReparent);

        intent.putExtra(IntentHandler.EXTRA_MULTI_TAB_REPARENTING_METADATA, multiTabBundle);
        IntentUtils.addTrustedIntentExtras(intent);
    }
}
