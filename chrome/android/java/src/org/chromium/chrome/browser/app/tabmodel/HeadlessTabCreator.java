// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreator.NeedsTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** This implementation always creates tabs as frozen/pending, never with web contents. */
public class HeadlessTabCreator extends TabCreator implements NeedsTabModel {
    private final Profile mProfile;
    private @Nullable TabModel mTabModel;

    public HeadlessTabCreator(Profile profile) {
        mProfile = profile;
    }

    @Override
    public void setTabModel(TabModel tabModel) {
        assert mTabModel == null;
        mTabModel = tabModel;
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent) {
        return createNewTab(loadUrlParams, /* title= */ "", type, parent, mTabModel.getCount());
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, int position) {
        return createNewTab(loadUrlParams, /* title= */ "", type, /* parent= */ null, position);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            String title,
            @TabLaunchType int type,
            Tab parent,
            int position) {
        Tab tab =
                TabBuilder.createForLazyLoad(mProfile, loadUrlParams, title)
                        .setLaunchType(type)
                        .setDelegateFactory(new HeadlessTabDelegateFactory())
                        .setParent(parent)
                        .build();
        mTabModel.addTab(tab, position, type, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        return tab;
    }

    @Override
    public Tab createFrozenTab(TabState state, int id, int index) {
        Tab tab =
                TabBuilder.createFromFrozenState(mProfile)
                        .setId(id)
                        .setDelegateFactory(new HeadlessTabDelegateFactory())
                        .setTabState(state)
                        .build();
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);
        return tab;
    }

    @Override
    public @Nullable Tab launchUrl(String url, @TabLaunchType int type) {
        return createNewTab(
                new LoadUrlParams(url),
                /* title= */ "",
                type,
                /* parent= */ null,
                mTabModel.getCount());
    }

    @Override
    public Tab createTabWithWebContents(
            @Nullable Tab parent,
            WebContents webContents,
            @TabLaunchType int type,
            GURL url,
            boolean addTabToModel) {
        throw new RuntimeException("Headless does not support live web contents.");
    }

    @Override
    public @Nullable Tab createTabWithHistory(@Nullable Tab parent, int type) {
        throw new RuntimeException("Headless does not support live web contents.");
    }
}
