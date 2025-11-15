// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
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
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** {@link NativePageNavigationDelegate} implementation. */
@NullMarked
public class NativePageNavigationDelegateImpl implements NativePageNavigationDelegate {
    private final Profile mProfile;

    protected final TabModelSelector mTabModelSelector;
    protected final Tab mTab;
    protected final Activity mActivity;
    protected final NativePageHost mHost;
    private final MultiInstanceManager mMultiInstanceManager;

    public NativePageNavigationDelegateImpl(
            Activity activity,
            Profile profile,
            NativePageHost host,
            TabModelSelector tabModelSelector,
            Tab tab,
            MultiInstanceManager multiInstanceManager) {
        mActivity = activity;
        mProfile = profile;
        mHost = host;
        mTabModelSelector = tabModelSelector;
        mTab = tab;
        mMultiInstanceManager = multiInstanceManager;
    }

    @Override
    public boolean isOpenInIncognitoEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled(mProfile);
    }

    @Override
    public boolean isOpenInAnotherWindowEnabled() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity)
                || MultiWindowUtils.getInstance().canEnterMultiWindowMode();
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
                // TODO(crbug.com/435490901): Update native page context menu to handle incognito
                // windows.
                if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
                    mMultiInstanceManager.openUrlInOtherWindow(
                            loadUrlParams,
                            mHost.getParentId(),
                            /* preferNew= */ false,
                            PersistedInstanceType.ACTIVE);
                } else {
                    openUrlInNewWindow(loadUrlParams);
                }
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
        Tab newTab =
                mTabModelSelector.openNewTab(
                        loadUrlParams,
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                        mTab,
                        /* incognito= */ false);
        return newTab;
    }

    private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(false);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                mActivity,
                mHost.getParentId(),
                MultiWindowUtils.getForegroundWindowActivity(mActivity),
                NewWindowAppSource.OTHER,
                /* preferNew= */ false);
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
            var bridge = assumeNonNull(OfflinePageBridge.getForProfile(mProfile));
            bridge.scheduleDownload(
                    mTab.getWebContents(),
                    OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE,
                    url,
                    DownloadUiActionFlags.ALL);
        } else {
            var bridge = assumeNonNull(RequestCoordinatorBridge.getForProfile(mProfile));
            bridge.savePageLater(
                    url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, /* userRequested= */ true);
        }
    }

    @Override
    public void initAndroidPrerenderManager(AndroidPrerenderManager androidPrerenderManager) {
        if (mTab != null) {
            androidPrerenderManager.initializeWithTab(mTab);
        }
    }
}
