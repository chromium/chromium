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
import org.chromium.chrome.browser.tabmodel.MultiTabMetadata;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;

import java.util.List;

/** Takes care of reparenting a list of Tab objects from one Activity to another. */
@NullMarked
public class ReparentingTabsTask {
    private final List<Tab> mTabs;

    /**
     * Creates a new task to reparent a list of tabs.
     *
     * @param tabs The list of {@link Tab} objects to reparent.
     * @return A new {@link ReparentingTabsTask} object.
     */
    public static ReparentingTabsTask from(List<Tab> tabs) {
        return new ReparentingTabsTask(tabs);
    }

    private ReparentingTabsTask(List<Tab> tabs) {
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
     * @param startActivityOptions Options to pass to {@link Activity#startActivity(Intent, Bundle)}
     * @param finalizeCallback A callback that will be called after the tab is attached to the new
     *     host activity in {@link #attachAndFinishReparenting}.
     */
    public void begin(
            @Nullable Context context,
            Intent intent,
            @Nullable Bundle startActivityOptions,
            @Nullable Runnable finalizeCallback) {
        if (context == null) return;
        setupIntent(intent, finalizeCallback);
        context.startActivity(intent, startActivityOptions);
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
        if (mTabs.size() == 1) {
            ReparentingTask.from(mTabs.get(0)).setupIntent(intent, finalizeCallback);
            return;
        }
        if (intent == null) intent = new Intent();
        if (intent.getComponent() == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);

        if (mTabs == null || mTabs.isEmpty()) return;

        boolean isIncognito = mTabs.get(0).isIncognito();

        for (Tab tab : mTabs) {
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

        IntentHandler.setMultiTabMetadata(intent, MultiTabMetadata.create(mTabs));
        IntentUtils.addTrustedIntentExtras(intent);
    }
}
