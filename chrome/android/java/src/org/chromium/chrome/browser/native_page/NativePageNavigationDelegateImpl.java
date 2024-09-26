// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/** {@link NativePageNavigationDelegate} implementation. */
public class NativePageNavigationDelegateImpl implements NativePageNavigationDelegate {
    private final Profile mProfile;
    private final TabGroupCreationDialogManager mTabGroupCreationDialogManager;

    protected final TabModelSelector mTabModelSelector;
    protected final Tab mTab;
    protected final Activity mActivity;
    protected final NativePageHost mHost;

    public NativePageNavigationDelegateImpl(
            Activity activity,
            Profile profile,
            NativePageHost host,
            TabModelSelector tabModelSelector,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            Tab tab) {
        mActivity = activity;
        mProfile = profile;
        mHost = host;
        mTabModelSelector = tabModelSelector;
        mTabGroupCreationDialogManager = tabGroupCreationDialogManager;
        mTab = tab;
    }

    @Override
    public boolean isOpenInIncognitoEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled(mProfile);
    }

    @Override
    public boolean isOpenInNewWindowEnabled() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity)
                || MultiWindowUtils.getInstance().canEnterMultiWindowMode(mActivity);
    }

    @Override
    public @Nullable Tab openUrl(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        Tab loadingTab = null;

        switch (windowOpenDisposition) {
            case WindowOpenDisposition.CURRENT_TAB:
                mHost.loadUrl(loadUrlParams, mTabModelSelector.isIncognitoSelected());
                loadingTab = mTab;
                break;
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
                loadingTab = openUrlInNewTab(loadUrlParams, windowOpenDisposition);
                break;
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                loadingTab = openUrlInNewTab(loadUrlParams, windowOpenDisposition);
                break;
            case WindowOpenDisposition.OFF_THE_RECORD:
                mHost.loadUrl(loadUrlParams, true);
                break;
            case WindowOpenDisposition.NEW_WINDOW:
                openUrlInNewWindow(loadUrlParams);
                break;
            case WindowOpenDisposition.SAVE_TO_DISK:
                saveUrlForOffline(loadUrlParams.getUrl());
                break;
            default:
                assert false;
        }

        return loadingTab;
    }

    @Override
    public Tab openUrlInGroup(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        boolean willMergingCreateNewGroup = filter.willMergingCreateNewGroup(List.of(mTab));
        Tab newTab =
                mTabModelSelector.openNewTab(
                        loadUrlParams,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        mTab,
                        /* incognito= */ false);
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()
                && willMergingCreateNewGroup
                && !TabGroupCreationDialogManager.shouldSkipGroupCreationDialog(
                        /* shouldShow= */ false)) {
            mTabGroupCreationDialogManager.showDialog(mTab.getRootId(), filter);
        }
        return newTab;
    }

    private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(false);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                mActivity,
                mHost.getParentId(),
                MultiWindowUtils.getAdjacentWindowActivity(mActivity));
    }

    private Tab openUrlInNewTab(LoadUrlParams loadUrlParams, int windowOpenDisposition) {
        int tabLaunchType = TabLaunchType.FROM_LONGPRESS_BACKGROUND;
        if (windowOpenDisposition == WindowOpenDisposition.NEW_FOREGROUND_TAB) {
            tabLaunchType = TabLaunchType.FROM_LONGPRESS_FOREGROUND;
        }
        return mTabModelSelector.openNewTab(
                loadUrlParams, tabLaunchType, mTab, /* incognito= */ false);
    }

    private void saveUrlForOffline(String url) {
        if (mTab != null) {
            OfflinePageBridge.getForProfile(mProfile)
                    .scheduleDownload(
                            mTab.getWebContents(),
                            OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE,
                            url,
                            DownloadUiActionFlags.ALL);
        } else {
            RequestCoordinatorBridge.getForProfile(mProfile)
                    .savePageLater(
                            url,
                            OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE,
                            /* userRequested= */ true);
        }
    }

    @Override
    public void initAndroidPrerenderManager(AndroidPrerenderManager androidPrerenderManager) {
        if (mTab != null) {
            androidPrerenderManager.initializeWithTab(mTab);
        }
    }
}
