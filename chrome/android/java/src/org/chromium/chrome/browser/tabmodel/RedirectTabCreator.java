// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** This class creates various kinds of new tabs in another window. */
@NullMarked
public class RedirectTabCreator extends ChromeTabCreator {
    public RedirectTabCreator(
            Activity activity,
            WindowAndroid nativeWindow,
            Supplier<TabDelegateFactory> tabDelegateFactory,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            boolean incognito,
            AsyncTabParamsManager asyncTabParamsManager,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            MultiInstanceManager multiInstanceManager) {
        super(
                activity,
                nativeWindow,
                tabDelegateFactory,
                profileProviderSupplier,
                incognito,
                asyncTabParamsManager,
                tabModelSelectorSupplier,
                compositorViewHolderSupplier,
                multiInstanceManager);
    }

    @SuppressWarnings("WrongConstant")
    @Override
    @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            @Nullable String title,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position,
            @Nullable Intent intent,
            boolean copyHistory) {
        // Clean up AsyncTabParams with the tab to reparent if any.
        mAsyncTabParamsManager.remove(IntentHandler.getTabId(intent));

        // Sanitize the url.
        GURL url = UrlFormatter.fixupUrl(loadUrlParams.getUrl());
        loadUrlParams.setUrl(url.getValidSpecOrEmpty());
        loadUrlParams.setTransitionType(
                getTransitionType(type, intent, loadUrlParams.getTransitionType()));

        ChromeAsyncTabLauncher chromeAsyncTabLauncher = new ChromeAsyncTabLauncher(mIncognito);
        Activity otherActivity =
                MultiWindowUtils.getForegroundWindowActivityWithProfileType(mActivity, mIncognito);
        chromeAsyncTabLauncher.launchTabInOtherWindow(
                loadUrlParams,
                mActivity,
                Tab.INVALID_TAB_ID,
                otherActivity,
                NewWindowAppSource.OTHER,
                /* preferNew= */ false);
        return null;
    }
}
