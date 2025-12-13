// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;

/** Asynchronously creates Tabs by creating/starting up Activities. */
@NullMarked
public class ChromeAsyncTabLauncher implements AsyncTabLauncher {
    private final boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public ChromeAsyncTabLauncher(boolean incognito) {
        mIsIncognito = incognito;
    }

    /**
     * @return Running Activity that owns the given Tab, null if the Activity couldn't be found.
     */
    private static @Nullable Activity getActivityForTabId(int id) {
        if (id == Tab.INVALID_TAB_ID) return null;

        Tab tab = TabWindowManagerSingleton.getInstance().getTabById(id);
        if (tab == null) return null;

        Context tabContext = tab.getContext();
        return (tabContext instanceof Activity) ? (Activity) tabContext : null;
    }

    /**
     * Creates a tab in another window in multi-window mode. This will only work if {@link
     * MultiWindowUtils#isOpenInOtherWindowSupported(Activity)} is true for the given activity.
     *
     * <p>The window in which the tab will be opened will depend on the following criteria:
     *
     * <ul>
     *   <li>If {@code otherActivity} is non-null, the tab will be opened in this window.
     *   <li>If {@code preferNew} is true, the tab will be attempted to be opened in a brand new
     *       window. At instance limit, this action will fail to open the tab.
     *   <li>If {@code preferNew} is false, the tab will be opened in a new activity created for a
     *       restored inactive instance. At instance limit, this action will fail to open the tab.
     * </ul>
     *
     * @param loadUrlParams Parameters specifying the URL to load and other navigation details.
     * @param activity The current {@link Activity}.
     * @param parentId The ID of the parent tab, or {@link Tab#INVALID_TAB_ID}.
     * @param otherActivity The activity to create a new tab in. This is non-null when we have a
     *     visible activity running adjacently.
     * @param newWindowSource The source of new window creation used for metrics.
     * @param preferNew Whether we should attempt to launch the tab in a brand new window.
     */
    public void launchTabInOtherWindow(
            LoadUrlParams loadUrlParams,
            Activity activity,
            int parentId,
            @Nullable Activity otherActivity,
            @NewWindowAppSource int newWindowSource,
            boolean preferNew) {
        Intent intent =
                createNewTabIntent(
                        new AsyncTabCreationParams(loadUrlParams),
                        parentId,
                        TabLaunchType.FROM_CHROME_UI);

        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(activity);
        if (targetActivity == null) return;

        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, activity, targetActivity);
        IntentUtils.addTrustedIntentExtras(intent);

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // If there is a Chrome window running adjacently, open the link in it.
            // Otherwise create a new window.
            if (otherActivity != null) {
                assert otherActivity instanceof ChromeTabbedActivity;
                ((ChromeTabbedActivity) otherActivity).onNewIntent(intent);
                return;
            }
            if (preferNew) intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        }
        MultiInstanceManager.onMultiInstanceModeStarted();
        if (!activity.isInMultiWindowMode() && !MultiWindowUtils.shouldOpenInAdjacentWindow()) {
            intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        }
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, mIsIncognito);
        activity.startActivity(intent);
        RecordHistogram.recordEnumeratedHistogram(
                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                newWindowSource,
                NewWindowAppSource.NUM_ENTRIES);
    }

    /**
     * Creates a new tab and loads the specified URL in it. This is a convenience method for {@link
     * #launchNewTab} with the default {@link LoadUrlParams} and no parent tab.
     *
     * @param url the URL to open.
     * @param type the type of action that triggered that launch. Determines how the tab is opened
     *     (for example, in the foreground or background).
     */
    public void launchUrl(String url, @TabLaunchType int type) {
        launchNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public void launchNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams);
        launchNewTab(asyncParams, type, parent == null ? Tab.INVALID_TAB_ID : parent.getId());
    }

    /**
     * Launches a Tab to host the given WebContents asynchronously.
     *
     * @param asyncParams Parameters to create the Tab with, including the URL.
     * @param type Information about how the tab was launched.
     * @param parentId ID of the parent tab, if it exists.
     */
    public void launchNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
        assert asyncParams != null;

        // Tabs shouldn't be launched in affiliated mode when a webcontents exists.
        assert !(type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                && asyncParams.getWebContents() != null);

        Intent intent =
                createNewTabIntent(asyncParams, parentId, type);
        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private Intent createNewTabIntent(
            AsyncTabCreationParams asyncParams, int parentId,
            @TabLaunchType int launchType) {
        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManagerSingleton.getInstance().add(assignedTabId, asyncParams);

        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse(asyncParams.getLoadUrlParams().getUrl()));

        addAsyncTabExtras(asyncParams, parentId, launchType, assignedTabId, intent);

        return intent;
    }

    private void addAsyncTabExtras(
            AsyncTabCreationParams asyncParams,
            int parentId,
            @TabLaunchType int launchType,
            int assignedTabId,
            Intent intent) {
        ComponentName componentName = asyncParams.getComponentName();
        if (componentName == null) {
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
        } else {
            ActivityUtils.setNonAliasedComponentForMainBrowsingActivity(intent, componentName);
        }
        IntentHandler.setIntentExtraHeaders(
                asyncParams.getLoadUrlParams().getExtraHeaders(), intent);

        IntentHandler.setTabId(intent, assignedTabId);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, mIsIncognito);
        intent.putExtra(IntentHandler.EXTRA_PARENT_TAB_ID, parentId);
        IntentHandler.setTabLaunchType(intent, launchType);

        boolean isChromeUi = (launchType == TabLaunchType.FROM_CHROME_UI);
        if (mIsIncognito || isChromeUi) {
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
        }

        if (isChromeUi) intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        Activity parentActivity = getActivityForTabId(parentId);
        if (parentActivity != null && parentActivity.getIntent() != null) {
            intent.putExtra(IntentHandler.EXTRA_PARENT_INTENT, parentActivity.getIntent());
        }

        if (asyncParams.getRequestId() != null) {
            intent.putExtra(
                    ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA,
                    asyncParams.getRequestId().intValue());
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    /**
     * Passes the supplied web app launch intent to the IntentHandler.
     *
     * @param intent Web app launch intent.
     */
    public void launchNewStandaloneFrame(Intent intent) {
        assert intent != null;
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentHandler.startActivityForTrustedIntent(intent);
    }
}
