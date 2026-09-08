// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.util.ArrayMap;
import android.util.ArraySet;

import org.chromium.base.Log;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.TabCache;
import org.chromium.chrome.browser.app.tabmodel.TabCacheKey;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateAttributesRegistry;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Profile-scoped pool managing background actor tabs.
 *
 * <p>Encapsulates in-memory {@link LiveBackgroundTab} instances and persisted cold tabs via {@link
 * TabCache}.
 */
@NullMarked
public class BackgroundTabPool
        implements Destroyable, TabStateAttributes.Observer, TabStateAttributes.StoreKey {
    private static final String TAG = "BgTabPool";
    private static final String ACTOR_DIR_TAG_PREFIX = "actor_tabs_";

    private final String mProfileToken;
    private final TabCache mTabCache;
    private final Runnable mOnEmptyCallback;
    private final ArrayMap<Integer, LiveBackgroundTab> mLiveEntries = new ArrayMap<>();
    private final ArrayMap<@TabId Integer, @TabId Integer> mPlaceholderToTabId = new ArrayMap<>();
    private boolean mIsDestroyed;

    /**
     * Constructs a {@link BackgroundTabPool} for the given profile token and on-empty callback.
     *
     * @param profileToken The token string associated with the profile.
     * @param onEmptyCallback Callback invoked when the pool transitions to empty.
     */
    public BackgroundTabPool(String profileToken, Runnable onEmptyCallback) {
        assertOnUiThread();
        mProfileToken = profileToken;
        mTabCache =
                TabCacheManager.create(
                        ACTOR_DIR_TAG_PREFIX + profileToken, /* cipherFactory= */ null);
        mOnEmptyCallback = onEmptyCallback;
        populatePlaceholderAssociations();
    }

    private void populatePlaceholderAssociations() {
        for (int cachedTabId : mTabCache.getAllTabIds()) {
            int placeholderTabId = BackgroundTabDataStore.getPlaceholderTabId(cachedTabId);
            if (placeholderTabId != Tab.INVALID_TAB_ID) {
                mPlaceholderToTabId.put(placeholderTabId, cachedTabId);
            }
        }
    }

    /** Returns the profile token associated with this pool. */
    public String getProfileToken() {
        checkNotDestroyed();
        return mProfileToken;
    }

    /** Returns whether the pool has zero live in-memory tabs and zero placeholder associations. */
    public boolean isEmpty() {
        checkNotDestroyed();
        return mLiveEntries.isEmpty() && mPlaceholderToTabId.isEmpty();
    }

    /** Returns the number of active live in-memory tabs in the pool. */
    public int getLiveTabCount() {
        checkNotDestroyed();
        return mLiveEntries.size();
    }

    /**
     * Adds a live in-memory tab to the pool, attaching {@link TabStateAttributes.Observer} and
     * saving its state if dirty.
     *
     * @param liveTab The {@link LiveBackgroundTab} to add.
     */
    public void addLiveTab(LiveBackgroundTab liveTab) {
        checkNotDestroyed();
        assert !mPlaceholderToTabId.containsKey(liveTab.getPlaceholderTabId())
                : "Placeholder already associated: " + liveTab.getPlaceholderTabId();
        Tab tab = liveTab.getTab();
        @TabId int tabId = tab.getId();
        @TabId int placeholderTabId = liveTab.getPlaceholderTabId();

        // Clean up any existing observer and entry before inserting.
        removeTabObserver(tab);
        mLiveEntries.put(tabId, liveTab);
        mPlaceholderToTabId.put(placeholderTabId, tabId);

        TabStateAttributes attributes = getTabStateAttributes(tab);
        if (attributes != null) {
            if (attributes.addObserver(this) != DirtinessState.CLEAN) {
                saveTabToCache(tab);
            }
        } else {
            saveTabToCache(tab);
        }
    }

    /**
     * Returns the {@link LiveBackgroundTab} for the given tab ID, or null if not live in memory.
     *
     * @param tabId The ID of the live tab to retrieve.
     * @return The {@link LiveBackgroundTab}, or null if not live.
     */
    public @Nullable LiveBackgroundTab getLiveTab(@TabId int tabId) {
        checkNotDestroyed();
        return mLiveEntries.get(tabId);
    }

    /**
     * Returns a set of all tab IDs managed by this pool (both in-memory live tabs and cached tabs).
     *
     * @return A {@link Set} of all {@link TabId} integers in the pool.
     */
    public Set<@TabId Integer> getAllTabIds() {
        checkNotDestroyed();
        Set<@TabId Integer> cachedIds = mTabCache.getAllTabIds();
        ArraySet<@TabId Integer> allIds = new ArraySet<>(cachedIds.size() + mLiveEntries.size());
        allIds.addAll(cachedIds);
        allIds.addAll(mLiveEntries.keySet());
        return Collections.unmodifiableSet(allIds);
    }

    /**
     * Returns whether the pool has a background tab associated with the given placeholder tab ID.
     *
     * @param placeholderTabId The placeholder tab ID.
     * @return True if the placeholder tab ID exists in the pool.
     */
    public boolean hasPlaceholder(@TabId int placeholderTabId) {
        checkNotDestroyed();
        return mPlaceholderToTabId.containsKey(placeholderTabId);
    }

    /**
     * Returns an unmodifiable set of all placeholder tab IDs managed by this pool.
     *
     * @return A {@link Set} of all placeholder {@link TabId} integers.
     */
    public Set<@TabId Integer> getAllPlaceholderTabIds() {
        checkNotDestroyed();
        return Collections.unmodifiableSet(mPlaceholderToTabId.keySet());
    }

    /**
     * Loads a tab from the pool by its placeholder tab ID, returning either a {@link
     * LiveBackgroundTab} or a deserialized {@link ColdBackgroundTab}.
     *
     * @param placeholderTabId The placeholder tab ID of the tab to load.
     * @return The {@link BackgroundPoolTab}, or null if loading failed.
     */
    public @Nullable BackgroundPoolTab loadTab(@TabId int placeholderTabId) {
        checkNotDestroyed();
        Integer tabId = mPlaceholderToTabId.get(placeholderTabId);
        if (tabId == null) {
            return null;
        }

        LiveBackgroundTab liveTab = mLiveEntries.remove(tabId);
        if (liveTab != null) {
            removeTabObserver(liveTab.getTab());
            notifyIfEmptied();
            return liveTab;
        }

        TabCacheKey key = getCacheKey(tabId);
        LoadedTabState loaded = mTabCache.getPreLoadedTabOrLoad(key);
        if (loaded != null && loaded.tabState != null) {
            return new ColdBackgroundTab(this, tabId, loaded.tabState, placeholderTabId);
        }
        Log.w(TAG, "Failed to load background tab %d from TabCache. Loaded: %s", tabId, loaded);
        return null;
    }

    /**
     * Removes a tab from live in-memory entries, placeholder mappings, and clears cached state.
     *
     * @param placeholderTabId The placeholder ID of the tab to remove.
     */
    public void removeTab(@TabId int placeholderTabId) {
        checkNotDestroyed();
        Integer tabId = mPlaceholderToTabId.remove(placeholderTabId);
        if (tabId != null) {
            LiveBackgroundTab tab = mLiveEntries.remove(tabId);
            if (tab != null) {
                removeTabObserver(tab.getTab());
            }
            mTabCache.clear(getCacheKey(tabId));
            BackgroundTabDataStore.deletePlaceholderTabId(tabId);
        }
        notifyIfEmptied();
    }

    /**
     * Asynchronously preloads tab states for a list of tab IDs.
     *
     * @param tabIds The list of {@link TabId}s to prefetch.
     */
    public void prefetchTabs(List<@TabId Integer> tabIds) {
        checkNotDestroyed();
        for (int i = 0; i < tabIds.size(); i++) {
            @TabId int tabId = tabIds.get(i);
            if (!mLiveEntries.containsKey(tabId)) {
                mTabCache.preloadTab(getCacheKey(tabId));
            }
        }
    }

    /** Clears all live and cached tabs in the pool. */
    public void clearAll() {
        checkNotDestroyed();
        for (int i = 0; i < mLiveEntries.size(); i++) {
            LiveBackgroundTab liveTab = mLiveEntries.valueAt(i);
            removeTabObserver(liveTab.getTab());
        }
        mLiveEntries.clear();
        mPlaceholderToTabId.clear();
        mTabCache.clearAll();
        notifyIfEmptied();
    }

    /** Cleans up on-disk persisted cache post restoration. */
    public void cleanupPostRestore() {
        checkNotDestroyed();
        mTabCache.clearAll();
    }

    @Override
    public void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness) {
        checkNotDestroyed();
        if (dirtiness == DirtinessState.DIRTY) {
            saveTabToCache(tab);
        }
    }

    @Override
    public void destroy() {
        checkNotDestroyed();
        mIsDestroyed = true;
        for (int i = 0; i < mLiveEntries.size(); i++) {
            LiveBackgroundTab liveTab = mLiveEntries.valueAt(i);
            removeTabObserver(liveTab.getTab());
        }
        mLiveEntries.clear();
        mPlaceholderToTabId.clear();
    }

    /** Returns whether this pool has been destroyed. */
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    private void notifyIfEmptied() {
        if (isEmpty() && !mIsDestroyed) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, mOnEmptyCallback);
        }
    }

    private void checkNotDestroyed() {
        assertOnUiThread();
        assert !mIsDestroyed : "BackgroundTabPool has already been destroyed.";
    }

    private void removeTabObserver(Tab tab) {
        if (!tab.isDestroyed()) {
            TabStateAttributes attributes = getTabStateAttributes(tab);
            if (attributes != null) {
                attributes.removeObserver(this);
            }
        }
    }

    private static @Nullable TabStateAttributes getTabStateAttributes(Tab tab) {
        if (tab.getUserDataHost() == null) {
            return null;
        }
        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(tab, BackgroundTabPool.class);
        if (attributes == null) {
            attributes =
                    TabStateAttributesRegistry.getAttributesFor(
                            tab, TabStateAttributes.StoreKey.class);
        }
        if (attributes == null) {
            attributes =
                    TabStateAttributesRegistry.getAttributesFor(tab, TabPersistentStoreImpl.class);
        }
        return attributes;
    }

    private void saveTabToCache(Tab tab) {
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState == null) {
            return;
        }
        mTabCache.saveTabState(getCacheKey(tab.getId()), tab.getId(), tabState);
    }

    private static TabCacheKey getCacheKey(@TabId int tabId) {
        return new TabCacheKey(String.valueOf(tabId), /* isIncognito= */ false);
    }
}
