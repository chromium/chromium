// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Creates tabs for the archived tab model selector during restore. This only creates frozen tabs.
 */
public class ArchivedTabCreator extends TabCreator {
    private final WindowAndroid mWindow;
    private TabModel mTabModel;

    /**
     * @param window The {@link AndroidWindow} to attach tabs to.
     */
    public ArchivedTabCreator(WindowAndroid window) {
        mWindow = window;
    }

    /**
     * @param tabModel The {@link TabModel} to add tabs to.
     */
    public void setTabModel(TabModel tabModel) {
        mTabModel = tabModel;
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent) {
        return createNewTab(loadUrlParams, type, parent, TabList.INVALID_TAB_INDEX);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, int index) {
        return createNewTab(loadUrlParams, /* title= */ null, type, parent, index);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            String title,
            @TabLaunchType int type,
            Tab parent,
            int index) {
        // TODO(crbug.com/331827001): Also possible to change the entire restore path.
        assert type == TabLaunchType.FROM_RESTORE
                : "ArchivedTabCreator only supports #createNewTab calls as a restore fallback.";
        Tab tab =
                TabBuilder.createForLazyLoad(mTabModel.getProfile(), loadUrlParams, title)
                        .setWindow(mWindow)
                        .setLaunchType(TabLaunchType.FROM_RESTORE)
                        .setTabResolver((tabId) -> mTabModel.getTabById(tabId))
                        .setInitiallyHidden(true)
                        .setDelegateFactory(CustomTabDelegateFactory.createEmpty())
                        .build();
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        return tab;
    }

    @Override
    public Tab createFrozenTab(TabState state, int id, int index) {
        assert mTabModel != null : "Creating frozen tab before native library initialized.";
        Tab tab =
                TabBuilder.createFromFrozenState(mTabModel.getProfile())
                        .setWindow(mWindow)
                        .setId(id)
                        .setLaunchType(TabLaunchType.FROM_RESTORE)
                        .setTabResolver((tabId) -> mTabModel.getTabById(tabId))
                        .setInitiallyHidden(true)
                        .setTabState(state)
                        .setDelegateFactory(CustomTabDelegateFactory.createEmpty())
                        .build();
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        return tab;
    }

    @Override
    public @Nullable Tab launchUrl(String url, @TabLaunchType int type) {
        assert false : "Not reached.";
        return null;
    }

    @Override
    public boolean createTabWithWebContents(
            @Nullable Tab parent,
            WebContents webContents,
            @TabLaunchType int type,
            @NonNull GURL url) {
        assert false : "Not reached.";
        return false;
    }
}
