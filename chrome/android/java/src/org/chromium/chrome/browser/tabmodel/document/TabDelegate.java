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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/**
 * Asynchronously creates Tabs by creating/starting up Activities.
 */
public class TabDelegate extends AsyncTabCreator {
    private final boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public TabDelegate(boolean incognito) {
        mIsIncognito = incognito;
    }

    /**
     * @return Running Activity that owns the given Tab, null if the Activity couldn't be found.
     */
    private static Activity getActivityForTabId(int id) {
        if (id == Tab.INVALID_TAB_ID) return null;

        Tab tab = TabWindowManagerSingleton.getInstance().getTabById(id);
        if (tab == null) return null;

        Context tabContext = tab.getContext();
        return (tabContext instanceof Activity) ? (Activity) tabContext : null;
    }

    @Override
    public boolean createsTabsAsynchronously() {
        return true;
    }

    /**
     * Creates a frozen Tab.  This Tab is not meant to be used or unfrozen -- it is only used as a
     * placeholder until the real Tab can be created.
     * The index is ignored in DocumentMode because Android handles the ordering of Tabs.
     */
    @Override
    public Tab createFrozenTab(TabState state, int id, boolean isIncognito, int index) {
        if (isIncognito != mIsIncognito) {
            throw new IllegalStateException("Incognito state mismatch. isIncognito: " + isIncognito
                    + ". TabDelegate: " + mIsIncognito);
        }
        return TabBuilder.createFromFrozenState().setId(id).setIncognito(isIncognito).build();
    }

    @Override
    public boolean createTabWithWebContents(@Nullable Tab parent, WebContents webContents,
            @TabLaunchType int type, @NonNull GURL url) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(
                new LoadUrlParams(url.getSpec(), PageTransition.AUTO_TOPLEVEL), webContents);
        createNewTab(asyncParams, type, parent != null ? parent.getId() : Tab.INVALID_TAB_ID);
        return true;
    }

    @Override
    public void createTabInOtherWindow(
            LoadUrlParams loadUrlParams, Activity activity, int parentId, Activity otherActivity) {
        Intent intent = createNewTabIntent(
                new AsyncTabCreationParams(loadUrlParams), parentId, false);

        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(activity);
        if (targetActivity == null) return;

        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, activity, targetActivity);
        IntentUtils.addTrustedIntentExtras(intent);

        MultiInstanceManager.onMultiInstanceModeStarted();
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            // If there is a Chrome window running adjacently, open the link in it.
            // Otherwise create a new window.
            if (otherActivity != null) {
                assert otherActivity instanceof ChromeTabbedActivity;
                ((ChromeTabbedActivity) otherActivity).onNewIntent(intent);
                return;
            }
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        }
        activity.startActivity(
                intent, MultiWindowUtils.getOpenInOtherWindowActivityOptions(activity));
    }

    @Override
    public Tab buildDetachedSpareTab(@TabLaunchType int type, boolean initializeRenderer) {
        return null;
    }

    @Override
    public Tab launchUrl(String url, @TabLaunchType int type) {
        return createNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams);
        createNewTab(asyncParams, type, parent == null ? Tab.INVALID_TAB_ID : parent.getId());
        return null;
    }

    @Override
    public Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, int position) {
        // Ignore position as it isn't supported for async creation.
        return createNewTab(loadUrlParams, type, parent);
    }

    @Override
    public void createNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
        assert asyncParams != null;

        // Tabs should't be launched in affiliated mode when a webcontents exists.
        assert !(type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                && asyncParams.getWebContents() != null);

        Intent intent = createNewTabIntent(
                asyncParams, parentId, type == TabLaunchType.FROM_CHROME_UI);
        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private Intent createNewTabIntent(
            AsyncTabCreationParams asyncParams, int parentId, boolean isChromeUI) {
        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManagerSingleton.getInstance().add(assignedTabId, asyncParams);

        Intent intent = new Intent(
                Intent.ACTION_VIEW, Uri.parse(asyncParams.getLoadUrlParams().getUrl()));

        addAsyncTabExtras(asyncParams, parentId, isChromeUI, assignedTabId, intent);

        return intent;
    }

    private void addAsyncTabExtras(AsyncTabCreationParams asyncParams, int parentId,
            boolean isChromeUI, int assignedTabId, Intent intent) {
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

        if (mIsIncognito || isChromeUI) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
        }

        if (isChromeUI) intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);

        Activity parentActivity = getActivityForTabId(parentId);
        if (parentActivity != null && parentActivity.getIntent() != null) {
            intent.putExtra(IntentHandler.EXTRA_PARENT_INTENT, parentActivity.getIntent());
        }

        if (asyncParams.getRequestId() != null) {
            intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA,
                    asyncParams.getRequestId().intValue());
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    @Override
    public void createNewStandaloneFrame(Intent intent) {
        assert intent != null;
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentHandler.startActivityForTrustedIntent(intent);
    }
}
