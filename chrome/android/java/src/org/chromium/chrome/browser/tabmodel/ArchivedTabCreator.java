// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabCreator.NeedsTabModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Creates tabs for the archived tab model selector during restore. This only creates frozen tabs.
 */
@NullMarked
public class ArchivedTabCreator implements TabCreator, NeedsTabModel {
    private final WindowAndroid mWindow;
    private TabModel mTabModel;

    /**
     * @param window The {@link AndroidWindow} to attach tabs to.
     */
    public ArchivedTabCreator(WindowAndroid window) {
        mWindow = window;
    }

    @Initializer
    @Override
    public void setTabModel(TabModel tabModel) {
        mTabModel = tabModel;
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
        return createNewTab(loadUrlParams, type, parent, TabList.INVALID_TAB_INDEX);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent, int index) {
        return createNewTab(loadUrlParams, /* title= */ null, type, parent, index);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            @Nullable String title,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int index) {
        // TODO(crbug.com/331827001): Also possible to change the entire restore path.
        assert type == TabLaunchType.FROM_RESTORE
                : "ArchivedTabCreator only supports #createNewTab calls as a restore fallback.";
        Tab tab =
                TabBuilder.createForLazyLoad(
                                assumeNonNull(mTabModel.getProfile()), loadUrlParams, title)
                        .setWindow(mWindow)
                        .setLaunchType(TabLaunchType.FROM_RESTORE)
                        .setTabResolver(mTabModel::getTabById)
                        .setInitiallyHidden(true)
                        .setDelegateFactory(CustomTabDelegateFactory.createEmpty())
                        .setArchived(true)
                        .build();
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        return tab;
    }

    @Override
    public @Nullable Tab createFrozenTab(TabState state, int id, int index) {
        assert mTabModel != null : "Creating frozen tab before native library initialized.";
        Tab tab =
                TabBuilder.createFromFrozenState(assumeNonNull(mTabModel.getProfile()))
                        .setWindow(mWindow)
                        .setId(id)
                        .setLaunchType(TabLaunchType.FROM_RESTORE)
                        .setTabResolver(mTabModel::getTabById)
                        .setInitiallyHidden(true)
                        .setTabState(state)
                        .setDelegateFactory(CustomTabDelegateFactory.createEmpty())
                        .setArchived(true)
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
    public Tab createTabWithWebContents(
            @Nullable Tab parent,
            boolean shouldPin,
            WebContents webContents,
            @TabLaunchType int type,
            GURL url,
            boolean addTabToModel) {
        assert false : "Not reached.";
        return assumeNonNull(null);
    }

    @Override
    public Tab createTabWithHistory(@Nullable Tab parent, int type) {
        assert false : "Not reached.";
        return assumeNonNull(null);
    }

    @Override
    public void launchNtp(@TabLaunchType int type) {
        TabCreatorUtil.launchNtp(this, getProfile(), type);
    }

    private Profile getProfile() {
        return assumeNonNull(mTabModel.getProfile());
    }
}
