// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.ui.widget.Toast;

/**
 * {@link NativePageNavigationDelegate} implementation.
 */
public class NativePageNavigationDelegateImpl implements NativePageNavigationDelegate {
    private static final String TAG = "PageNavDelegate";
    private final Profile mProfile;
    private final TabModelSelector mTabModelSelector;

    protected final ChromeActivity mActivity;
    protected final NativePageHost mHost;

    public NativePageNavigationDelegateImpl(ChromeActivity activity, Profile profile,
            NativePageHost host, TabModelSelector tabModelSelector) {
        mActivity = activity;
        mProfile = profile;
        mHost = host;
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public boolean isOpenInNewWindowEnabled() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity);
    }

    @Override
    @Nullable
    public Tab openUrl(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        Tab loadingTab = null;

        switch (windowOpenDisposition) {
            case WindowOpenDisposition.CURRENT_TAB:
                mHost.loadUrl(loadUrlParams, mTabModelSelector.isIncognitoSelected());
                loadingTab = mHost.getActiveTab();
                break;
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                loadingTab = openUrlInNewTab(loadUrlParams);
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

    private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
        TabDelegate tabDelegate = new TabDelegate(false);
        tabDelegate.createTabInOtherWindow(loadUrlParams, mActivity, mHost.getParentId());
    }

    private Tab openUrlInNewTab(LoadUrlParams loadUrlParams) {
        Tab tab = mTabModelSelector.openNewTab(loadUrlParams,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, mHost.getActiveTab(),
                /* incognito = */ false);

        // If animations are disabled in the DeviceClassManager, a toast is already displayed for
        // all tabs opened in the background.
        // TODO(twellington): Replace this with an animation.
        BottomSheetController controller = mActivity.getBottomSheetController();
        if (controller != null && controller.getSheetState() == SheetState.FULL
                && DeviceClassManager.enableAnimations()) {
            Toast.makeText(mActivity, R.string.open_in_new_tab_toast, Toast.LENGTH_SHORT).show();
        }

        return tab;
    }

    private void saveUrlForOffline(String url) {
        if (mHost.getActiveTab() != null) {
            OfflinePageBridge.getForProfile(mProfile).scheduleDownload(
                    mHost.getActiveTab().getWebContents(),
                    OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, url, DownloadUiActionFlags.ALL);
        } else {
            RequestCoordinatorBridge.getForProfile(mProfile).savePageLater(
                    url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* userRequested */);
        }
    }
}
