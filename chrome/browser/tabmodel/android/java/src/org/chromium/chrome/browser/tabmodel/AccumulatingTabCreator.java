// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link TabCreator} that doesn't create anything, always returning nulls. But it records and
 * remembers all of the arguments that have been passed to it. This is useful if we wanted to create
 * things later, or if we wanted to compare against another implementation such as when double
 * reading on start up to compare two storage implementations.
 */
@NullMarked
public class AccumulatingTabCreator implements TabCreator {
    public static class CreateNewTabArguments {
        public final LoadUrlParams loadUrlParams;
        public final @Nullable String title;
        public final @TabLaunchType int tabLaunchType;
        public final @Nullable Tab parent;
        public final int position;

        public CreateNewTabArguments(
                LoadUrlParams loadUrlParams,
                @Nullable String title,
                @TabLaunchType int tabLaunchType,
                @Nullable Tab parent,
                int position) {
            this.loadUrlParams = loadUrlParams;
            this.title = title;
            this.tabLaunchType = tabLaunchType;
            this.parent = parent;
            this.position = position;
        }
    }

    public static class CreateFrozenTabArguments {
        public final TabState state;
        public final int id;
        public final int index;

        public CreateFrozenTabArguments(TabState state, int id, int index) {
            this.state = state;
            this.id = id;
            this.index = index;
        }
    }

    public final List<CreateNewTabArguments> createNewTabArgumentsList = new ArrayList<>();
    public final List<CreateFrozenTabArguments> createFrozenTabArgumentsList = new ArrayList<>();

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
        return createNewTab(loadUrlParams, type, parent, TabModel.INVALID_TAB_INDEX);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position) {
        return createNewTab(loadUrlParams, /* title= */ "", type, parent, position);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            String title,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position) {
        createNewTabArgumentsList.add(
                new CreateNewTabArguments(loadUrlParams, title, type, parent, position));
        return null;
    }

    @Override
    public @Nullable Tab createFrozenTab(TabState state, int id, int index) {
        createFrozenTabArgumentsList.add(new CreateFrozenTabArguments(state, id, index));
        return null;
    }

    @Override
    public @Nullable Tab launchUrl(String url, @TabLaunchType int type) {
        // Should never be called.
        assert false;
        return null;
    }

    @Override
    public @Nullable Tab createTabWithWebContents(
            @Nullable Tab parent,
            boolean shouldPin,
            WebContents webContents,
            @TabLaunchType int type,
            GURL url,
            boolean addTabToModel) {
        // Should never be called.
        assert false;
        return null;
    }

    @Override
    public @Nullable Tab createTabWithHistory(Tab parent, @TabLaunchType int type) {
        // Should never be called.
        assert false;
        return null;
    }

    @Override
    public void launchNtp(@TabLaunchType int type) {
        // Should never be called.
        assert false;
    }
}
