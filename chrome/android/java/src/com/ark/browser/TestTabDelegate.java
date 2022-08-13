// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.state.SerializedCriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

public class TestTabDelegate extends TabCreator {

    private WindowAndroid mNativeWindow;

    public TestTabDelegate(WindowAndroid mNativeWindow) {
        this.mNativeWindow = mNativeWindow;
    }

    @Override
    public boolean createsTabsAsynchronously() {
        return false;
    }

    /**
     * Creates a frozen Tab.  This Tab is not meant to be used or unfrozen -- it is only used as a
     * placeholder until the real Tab can be created.
     * The index is ignored in DocumentMode because Android handles the ordering of Tabs.
     */
    @Override
    public Tab createFrozenTab(TabState state,
            SerializedCriticalPersistedTabData criticalPersistedTabData, int id,
            boolean isIncognito, int index) {
        return TabBuilder.createFromFrozenState().setId(id).setIncognito(false).build();
    }

    @Override
    public boolean createTabWithWebContents(@Nullable Tab parent, WebContents webContents,
            @TabLaunchType int type, @NonNull GURL url) {
        Tab tab = TabBuilder.createLiveTab(false)
                .setParent(parent)
                .setIncognito(false)
                .setWindow(mNativeWindow)
                .setLaunchType(type)
                .setWebContents(webContents)
//                .setDelegateFactory(new TabDelegateFactory() {
//                    @Override
//                    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
//                        return null;
//                    }
//
//                    @Override
//                    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
//                        return null;
//                    }
//
//                    @Override
//                    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
//                        return null;
//                    }
//
//                    @Override
//                    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
//                        return null;
//                    }
//                })
                .setInitiallyHidden(false)
                .build();
        return true;
    }

    @Override
    public Tab launchUrl(String url, @TabLaunchType int type) {
        return createNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public Tab createNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent) {
        Tab tab = TabBuilder.createLiveTab(false)
                .setParent(parent)
                .setIncognito(false)
                .setWindow(mNativeWindow)
                .setLaunchType(type)
//                .setDelegateFactory(delegateFactory)
                .setInitiallyHidden(false)
                .build();
        tab.loadUrl(loadUrlParams);
        return tab;
    }

}
