// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Takes care of reparenting a Tab object from one Activity to another. */
public class ReparentingTask implements UserData {
    public static final String TAG = "ReparentingTask";

    /** Provides data to {@link ReparentingTask} facilitate reparenting tabs. */
    public interface Delegate {
        /**
         * Gets a {@link CompositorViewHolder} which is passed on to {@link ReparentingTask}, used
         * in the reparenting process.
         *
         * <p>Can be null if the CompositorViewHolder does not yet exist.
         */
        @Nullable
        CompositorViewHolder getCompositorViewHolder();

        /**
         * Gets a {@link WindowAndroid} which is passed on to {@link ReparentingTask}, used in the
         * reparenting process.
         */
        WindowAndroid getWindowAndroid();

        /**
         * Gets a {@link TabDelegateFactory} which is passed on to {@link ReparentingTask}, used in
         * the reparenting process.
         */
        TabDelegateFactory getTabDelegateFactory();
    }

    private static final Class<ReparentingTask> USER_DATA_KEY = ReparentingTask.class;

    private final Tab mTab;

    /**
     * @param tab {@link Tab} object.
     * @return {@link ReparentingTask} object for a given {@link Tab}. Creates one
     *         if not present.
     */
    public static ReparentingTask from(Tab tab) {
        ReparentingTask reparentingTask = get(tab);
        if (reparentingTask == null) {
            reparentingTask =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new ReparentingTask(tab));
        }
        return reparentingTask;
    }

    public static @Nullable ReparentingTask get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private ReparentingTask(Tab tab) {
        mTab = tab;
    }

    /**
     * Begins the tab reparenting process. Detaches the tab from its current activity and fires
     * an Intent to reparent the tab into its new host activity.
     *
     * @param context {@link Context} object used to start a new activity.
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *               launching the new host activity. This intent's URI and action will be
     *               overridden. This may be null if no intent customization is needed.
     * @param startActivityOptions Options to pass to {@link Activity#startActivity(Intent, Bundle)}
     * @param finalizeCallback A callback that will be called after the tab is attached to the new
     *                         host activity in {@link #attachAndFinishReparenting}.
     */
    public void begin(
            Context context,
            Intent intent,
            Bundle startActivityOptions,
            Runnable finalizeCallback) {
        setupIntent(context, intent, finalizeCallback);
        context.startActivity(intent, startActivityOptions);
    }

    /**
     * Sets up the given intent to be used for reparenting a tab.
     * @param context {@link Context} object used to start a new activity.
     * @param intent An optional intent with the desired component, flags, or extras to use when
     *               launching the new host activity. This intent's URI and action will be
     *               overridden. This may be null if no intent customization is needed.
     * @param finalizeCallback A callback that will be called after the tab is attached to the new
     *                         host activity in {@link #attachAndFinishReparenting}.
     */
    public void setupIntent(Context context, Intent intent, Runnable finalizeCallback) {
        if (intent == null) intent = new Intent();
        if (intent.getComponent() == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        }
        intent.setAction(Intent.ACTION_VIEW);
        if (TextUtils.isEmpty(intent.getDataString())) {
            intent.setData(Uri.parse(mTab.getUrl().getSpec()));
        }
        if (mTab.isIncognito()) {
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }
        IntentUtils.addTrustedIntentExtras(intent);

        // Add the tab to AsyncTabParamsManager before removing it from the current model to
        // ensure the global count of tabs is correct. See https://crbug.com/611806.
        IntentHandler.setTabId(intent, mTab.getId());
        AsyncTabParamsManagerSingleton.getInstance()
                .add(mTab.getId(), new TabReparentingParams(mTab, finalizeCallback));

        detach();
    }

    /**
     * Detaches a tab from its current activity if any.
     *
     * In details, this function:
     * - Removes the tab from its current {@link TabModelSelector}, effectively severing
     *   the {@link Activity} to {@link Tab} link.
     */
    public void detach() {
        // TODO(yusufo): We can't call tab.updateWindowAndroid that sets |mWindowAndroid| to null
        // because many code paths (including navigation) expect the tab to always be associated
        // with an activity, and will crash. crbug.com/657007
        WebContents webContents = mTab.getWebContents();

        // TODO(crbug.com/40067160): We shouldn't be detaching tabs with null WebContents as it can
        // put the tab into an unexpected detached = false state if a navigation happens on the
        // detached tab.
        if (webContents != null) {
            webContents.setTopLevelNativeWindow(null);
        } else {
            Log.e(TAG, "WebContents was null when detaching a tab for reparenting.");
        }

        // TabModelSelector of this Tab, if present, gets notified to remove the tab from
        // the TabModel it belonged to.
        mTab.updateAttachment(null, null);
    }

    /**
     * Finishes the tab reparenting process. Attaches this tab to a new activity, and updates the
     * tab and related objects to reference it. This updates many delegates inside the tab and
     * {@link WebContents} both on java and native sides.
     *
     * @param delegate A delegate that provides dependencies.
     * @param finalizeCallback A Callback to be called after the Tab has been reparented.
     */
    public void finish(@NonNull Delegate delegate, @Nullable Runnable finalizeCallback) {
        if (delegate.getCompositorViewHolder() != null) {
            delegate.getCompositorViewHolder().prepareForTabReparenting();
        }
        attach(delegate.getWindowAndroid(), delegate.getTabDelegateFactory());
        if (finalizeCallback != null) finalizeCallback.run();
    }

    /**
     * Attaches the tab to the new activity and updates the tab and related objects to reference the
     * new activity. This updates many delegates inside the tab and {@link WebContents} both on
     * java and native sides.
     *
     * @param window A new {@link WindowAndroid} to attach the tab to.
     * @param tabDelegateFactory  The new delegate factory this tab should be using.
     */
    private void attach(WindowAndroid window, TabDelegateFactory tabDelegateFactory) {
        // Assert that the tab is currently in detached state.
        assert mTab.getWebContents() == null
                || mTab.getWebContents().getTopLevelNativeWindow() == null;
        mTab.updateAttachment(window, tabDelegateFactory);
        if (mTab.getWebContents() == null) return;
        ReparentingTaskJni.get().attachTab(mTab.getWebContents());
    }

    @NativeMethods
    interface Natives {
        void attachTab(WebContents webContents);
    }
}
