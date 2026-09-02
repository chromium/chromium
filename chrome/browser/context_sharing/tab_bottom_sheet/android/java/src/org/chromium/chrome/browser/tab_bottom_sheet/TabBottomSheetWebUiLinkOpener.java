// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate.LinkOpener;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Helper delegate for opening context menu links in new tabs or windows. */
@NullMarked
class TabBottomSheetWebUiLinkOpener implements LinkOpener {
    private final WindowAndroid mWindowAndroid;
    private final @Nullable WebContents mWebContents;

    TabBottomSheetWebUiLinkOpener(WindowAndroid windowAndroid, @Nullable WebContents webContents) {
        mWindowAndroid = windowAndroid;
        mWebContents = webContents;
    }

    private void safeStartActivity(Intent intent) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            activity.startActivity(intent);
        }
    }

    @Override
    public void openInNewTab(
            GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url.getSpec()));
        safeStartActivity(intent);
    }

    @Override
    public void openInNewTabInGroup(
            GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams) {
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
        if (selector != null && currentTab != null) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
            loadUrlParams.setAdditionalNavigationParams(additionalNavigationParams);
            selector.openNewTab(
                    loadUrlParams,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                    currentTab,
                    currentTab.isIncognito());
        } else {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(url.getSpec()));
            intent.putExtra(
                    BrowserIntentUtils.EXTRA_TAB_LAUNCH_TYPE,
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP);
            safeStartActivity(intent);
        }
    }

    @Override
    public void openInNewIncognitoTab(GURL url) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url.getSpec()));
        intent.putExtra(BrowserIntentUtils.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        safeStartActivity(intent);
    }

    @Override
    public void openInNewWindow(
            GURL url, @Nullable AdditionalNavigationParams additionalNavigationParams) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
            loadUrlParams.setAdditionalNavigationParams(additionalNavigationParams);
            MultiInstanceOrchestratorFactory.getInstance()
                    .openUrlInOtherWindow(
                            activity,
                            loadUrlParams,
                            Tab.INVALID_TAB_ID,
                            /* preferNew= */ true,
                            /* isIncognito= */ false);
        }
    }

    @Override
    public void openInIncognitoWindow(GURL url) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
            MultiInstanceOrchestratorFactory.getInstance()
                    .openUrlInOtherWindow(
                            activity,
                            loadUrlParams,
                            Tab.INVALID_TAB_ID,
                            /* preferNew= */ false,
                            /* isIncognito= */ true);
        }
    }

    @Override
    public boolean isIncognitoSupported() {
        if (mWebContents == null) return false;
        Profile profile = Profile.fromWebContents(mWebContents);
        return profile != null && IncognitoUtils.isIncognitoModeEnabled(profile);
    }
}
