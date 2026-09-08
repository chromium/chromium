// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;

/**
 * Cold tab representation instantiated on demand by {@link BackgroundTabPool#loadTab(int)} when
 * restoring persisted Actor tab state from TabCache.
 */
@NullMarked
public class ColdBackgroundTab implements BackgroundPoolTab {
    private final BackgroundTabPool mPool;
    private final @TabId int mTabId;
    private final @TabId int mPlaceholderTabId;
    private @Nullable TabState mTabState;

    /**
     * Constructs a {@link ColdBackgroundTab}.
     *
     * @param pool The {@link BackgroundTabPool} that owns this tab.
     * @param tabId The unique tab ID.
     * @param tabState The serialized tab state to restore from.
     * @param placeholderTabId The placeholder tab ID associated with this background tab.
     */
    public ColdBackgroundTab(
            BackgroundTabPool pool,
            @TabId int tabId,
            TabState tabState,
            @TabId int placeholderTabId) {
        mPool = pool;
        mTabId = tabId;
        mTabState = tabState;
        mPlaceholderTabId = placeholderTabId;
    }

    @Override
    public @TabId int getPlaceholderTabId() {
        return mPlaceholderTabId;
    }

    @Override
    public Tab attachTabImpl(TabModel tabModel, int index) {
        assert mTabState != null : "ColdBackgroundTab has already been attached or destroyed.";
        mPool.removeTab(mPlaceholderTabId);
        TabState state = mTabState;
        mTabState = null;
        TabCreator tabCreator = tabModel.getTabCreator();
        return assertNonNull(tabCreator.createFrozenTab(state, mTabId, index));
    }

    @Override
    public void destroy() {
        if (mTabState != null) {
            WebContentsState contentsState = mTabState.contentsState;
            if (contentsState != null) {
                contentsState.destroy();
            }
            mTabState = null;
        }
    }
}
