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
import org.chromium.content_public.common.Referrer;
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

    private LoadUrlParams createLoadUrlParams(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams,
            boolean targetIsIncognito) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
        // Only forward referrer and initiator navigation state when the source and destination
        // share the same incognito state, avoiding leaks across the privacy boundary.
        if (mWebContents != null
                && !mWebContents.isDestroyed()
                && mWebContents.isIncognito() == targetIsIncognito) {
            loadUrlParams.setReferrer(referrer);
            loadUrlParams.setAdditionalNavigationParams(additionalNavigationParams);
        } else if (additionalNavigationParams != null) {
            additionalNavigationParams.destroy();
        }
        return loadUrlParams;
    }

    @Override
    public void openInNewTab(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
        if (selector != null) {
            LoadUrlParams loadUrlParams =
                    createLoadUrlParams(
                            url,
                            referrer,
                            additionalNavigationParams,
                            /* targetIsIncognito= */ false);
            Tab newTab =
                    selector.openNewTab(
                            loadUrlParams,
                            TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                            currentTab != null && !currentTab.isIncognito() ? currentTab : null,
                            /* incognito= */ false);
            var remainingParams = loadUrlParams.getAdditionalNavigationParams();
            if (newTab == null && remainingParams != null) {
                remainingParams.destroy();
            }
        } else {
            if (additionalNavigationParams != null) {
                additionalNavigationParams.destroy();
            }
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(url.getSpec()));
            if (referrer != null) {
                intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrer.getUrl()));
            }
            safeStartActivity(intent);
        }
    }

    @Override
    public void openInNewTabInGroup(
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
        if (selector != null && currentTab != null) {
            boolean isIncognito = currentTab.isIncognito();
            LoadUrlParams loadUrlParams =
                    createLoadUrlParams(url, referrer, additionalNavigationParams, isIncognito);
            Tab newTab =
                    selector.openNewTab(
                            loadUrlParams,
                            TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                            currentTab,
                            isIncognito);
            var remainingParams = loadUrlParams.getAdditionalNavigationParams();
            if (newTab == null && remainingParams != null) {
                remainingParams.destroy();
            }
        } else {
            if (additionalNavigationParams != null) {
                additionalNavigationParams.destroy();
            }
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(url.getSpec()));
            if (referrer != null) {
                intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrer.getUrl()));
            }
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
            GURL url,
            @Nullable Referrer referrer,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            LoadUrlParams loadUrlParams =
                    createLoadUrlParams(
                            url,
                            referrer,
                            additionalNavigationParams,
                            /* targetIsIncognito= */ false);
            MultiInstanceOrchestratorFactory.getInstance()
                    .openUrlInOtherWindow(
                            activity,
                            loadUrlParams,
                            Tab.INVALID_TAB_ID,
                            /* preferNew= */ true,
                            /* isIncognito= */ false);
        } else if (additionalNavigationParams != null) {
            additionalNavigationParams.destroy();
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
