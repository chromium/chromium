// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

/**
 * A {@link TabCreator} that delegates to another {@link TabCreator} while recording the properties
 * of tabs it creates. This replaces the need for using the live {@link TabModel} when diffing tab
 * state stores.
 */
@NullMarked
public class RecordingTabCreator implements TabCreator {
    /** A data class that holds the properties of a tab that has been created. */
    public static class TabCreationData {
        public final int id;
        public final @Nullable String url;
        public final long timestampMillis;

        /**
         * @param id The ID of the tab (if frozen, otherwise Tab.INVALID_TAB_ID).
         * @param url The URL spec of the tab.
         * @param timestampMillis The timestamp of the tab in milliseconds (if frozen, otherwise 0).
         */
        public TabCreationData(int id, @Nullable String url, long timestampMillis) {
            this.id = id;
            this.url = url;
            this.timestampMillis = timestampMillis;
        }
    }

    private final List<TabCreationData> mFrozenTabCreationData = new ArrayList<>();
    private final List<TabCreationData> mNewTabCreationData = new ArrayList<>();
    private @Nullable TabCreator mDelegate;
    private int mTabCount;

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
        assertInitialized();
        recordNewTab(loadUrlParams.getUrl());
        return mDelegate.createNewTab(loadUrlParams, type, parent);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position) {
        assertInitialized();
        recordNewTab(loadUrlParams.getUrl());
        return mDelegate.createNewTab(loadUrlParams, type, parent, position);
    }

    @Override
    public @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            String title,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position) {
        assertInitialized();
        recordNewTab(loadUrlParams.getUrl());
        return mDelegate.createNewTab(loadUrlParams, title, type, parent, position);
    }

    @Override
    public @Nullable Tab createFrozenTab(TabState state, int id, int index) {
        assertInitialized();
        if (TabStateStorageFlagHelper.isTabStorageEnabled()) {
            mTabCount++;
            String urlSpec = state.url != null ? state.url.getSpec() : null;
            mFrozenTabCreationData.add(new TabCreationData(id, urlSpec, state.timestampMillis));
        }
        return mDelegate.createFrozenTab(state, id, index);
    }

    @Override
    public @Nullable Tab launchUrl(String url, @TabLaunchType int type) {
        assertInitialized();
        recordNewTab(url);
        return mDelegate.launchUrl(url, type);
    }

    @Override
    public @Nullable Tab createTabWithWebContents(
            @Nullable Tab parent,
            boolean shouldPin,
            WebContents webContents,
            @TabLaunchType int type,
            GURL url,
            int index,
            CompletableFuture<Boolean> addTabToModel) {
        assertInitialized();
        recordNewTab(url.getSpec());
        return mDelegate.createTabWithWebContents(
                parent, shouldPin, webContents, type, url, index, addTabToModel);
    }

    @Override
    public @Nullable Tab createTabWithHistory(Tab parent, @TabLaunchType int type) {
        assertInitialized();
        recordNewTab(parent.getUrl() != null ? parent.getUrl().getSpec() : null);
        return mDelegate.createTabWithHistory(parent, type);
    }

    @Override
    public void launchNtp(@TabLaunchType int type) {
        assertInitialized();
        recordNewTab(null);
        mDelegate.launchNtp(type);
    }

    /** Returns the total number of tabs created. */
    public int getTabCount() {
        return mTabCount;
    }

    /** Returns the list of frozen tab creation data. */
    public List<TabCreationData> getFrozenTabCreationData() {
        return mFrozenTabCreationData;
    }

    /** Returns the list of new tab creation data. */
    public List<TabCreationData> getNewTabCreationData() {
        return mNewTabCreationData;
    }

    /** Sets the delegate {@link TabCreator} to use. */
    public void setDelegate(TabCreator delegate) {
        mDelegate = delegate;
    }

    @EnsuresNonNull({"mDelegate"})
    private void assertInitialized() {
        assert mDelegate != null;
    }

    private void recordNewTab(@Nullable String urlSpec) {
        if (TabStateStorageFlagHelper.isTabStorageEnabled()) {
            mTabCount++;
            mNewTabCreationData.add(
                    new TabCreationData(Tab.INVALID_TAB_ID, urlSpec, /* timestampMillis= */ 0));
        }
    }
}
