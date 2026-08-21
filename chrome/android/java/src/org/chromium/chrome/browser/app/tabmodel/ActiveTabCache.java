// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Responsible for caching the active tab's state by coordinating window active-tab tracking and
 * delegating disk/FlatBuffer persistence to {@link TabCache}.
 *
 * <p>Note: ActiveTabCache cannot be profile-scoped since it is meant to be available pre-native.
 * Instead, it is logically partitioned per window and per model type using {@link TabCacheKey}
 * (keyed by {@code windowTag} and {@code isIncognito} boolean) with {@link CipherFactory} providing
 * incognito encryption. Do NOT introduce dependencies on {@code Profile}.
 */
@NullMarked
public class ActiveTabCache {
    public static final String CACHE_TAG = "active_tabs";

    /** Creates an instance of {@link ActiveTabCache}. */
    @FunctionalInterface
    public interface Factory {
        /**
         * @param windowTag The tag for the window being tracked.
         * @param selector The selector associated with the window.
         * @param cipherFactory The cipher factory for the store.
         */
        ActiveTabCache build(
                String windowTag, TabModelSelector selector, @Nullable CipherFactory cipherFactory);
    }

    private final Callback<@Nullable Tab> mOnRegularActiveTabChanged =
            (tab) -> onActiveTabChanged(false, tab);
    private final Callback<@Nullable Tab> mOnIncognitoActiveTabChanged =
            (tab) -> onActiveTabChanged(true, tab);

    private final TabModelSelector mTabModelSelector;
    private final @Nullable CipherFactory mCipherFactory;

    private final TabCacheKey mRegularTabCacheKey;
    private final TabCacheKey mIncognitoTabCacheKey;
    private final TabCache mTabCache;

    /**
     * @param windowTag The tag for the window being tracked.
     * @param selector The {@link TabModelSelector} to track.
     * @param cipherFactory The {@link CipherFactory} used to encrypt the incognito file.
     */
    public ActiveTabCache(
            String windowTag, TabModelSelector selector, @Nullable CipherFactory cipherFactory) {
        assertOnUiThread();
        mTabModelSelector = selector;
        mCipherFactory = cipherFactory;
        mRegularTabCacheKey = new TabCacheKey(windowTag, /* isIncognito= */ false);
        mIncognitoTabCacheKey = new TabCacheKey(windowTag, /* isIncognito= */ true);
        mTabCache = TabCacheManager.create(CACHE_TAG, cipherFactory);

        if (cipherFactory == null) {
            clearActiveTab(/* incognito= */ true);
        }

        mTabCache.preloadTab(mRegularTabCacheKey);
        if (cipherFactory != null) {
            mTabCache.preloadTab(mIncognitoTabCacheKey);
        }
    }

    /**
     * Saves the active tab's state.
     *
     * @param tab The active tab whose state is being saved.
     */
    public void saveActiveTab(Tab tab) {
        assertOnUiThread();
        boolean isOffTheRecord = tab.isOffTheRecord();
        assert !isOffTheRecord || mCipherFactory != null;

        TabCacheKey key = isOffTheRecord ? mIncognitoTabCacheKey : mRegularTabCacheKey;
        mTabCache.saveTab(key, tab);
    }

    /**
     * Restores the active tab from the cache. If it doesn't exist or failed to restore, return
     * null.
     *
     * @param incognito Whether to restore the incognito active tab.
     */
    public @Nullable LoadedTabState getPreLoadedActiveTabOrLoad(boolean incognito) {
        assertOnUiThread();
        TabCacheKey key = incognito ? mIncognitoTabCacheKey : mRegularTabCacheKey;
        return mTabCache.getPreLoadedTabOrLoad(key);
    }

    public void startTracking(boolean incognito) {
        assertOnUiThread();
        mTabCache.cancelAllTasks();

        TabModel model = mTabModelSelector.getModel(incognito);

        NullableObservableSupplier<Tab> currentTabSupplier = model.getCurrentTabSupplier();
        Callback<@Nullable Tab> onActiveTabChanged = getActiveTabChangedCallback(incognito);

        currentTabSupplier.addSyncObserverAndCallIfNonNull(onActiveTabChanged);
        onActiveTabChanged.onResult(currentTabSupplier.get());
    }

    public void stopTracking(boolean incognito) {
        assertOnUiThread();
        TabModel model = mTabModelSelector.getModel(incognito);
        NullableObservableSupplier<Tab> currentTabSupplier = model.getCurrentTabSupplier();
        currentTabSupplier.removeObserver(getActiveTabChangedCallback(incognito));
    }

    /**
     * Clears the active tab cache for the given incognito state.
     *
     * @param incognito Whether to clear the incognito or regular active tab.
     */
    public void clearActiveTab(boolean incognito) {
        assertOnUiThread();
        TabCacheKey key = incognito ? mIncognitoTabCacheKey : mRegularTabCacheKey;
        mTabCache.clear(key);
    }

    /** Clears all active tab cache for the current window. */
    public void clearCurrentWindow() {
        clearActiveTab(false);
        clearActiveTab(true);
    }

    /**
     * Cleans up the active tab cache for the given window tag.
     *
     * @param windowTag The window tag to clean up.
     */
    public static void cleanupWindow(String windowTag) {
        TabCache.cleanup(CACHE_TAG, new TabCacheKey(windowTag, /* isIncognito= */ false));
        TabCache.cleanup(CACHE_TAG, new TabCacheKey(windowTag, /* isIncognito= */ true));
    }

    /** Clears all active tab cache global state. */
    public static void clearGlobalState() {
        assertOnUiThread();
        TabCache.clearGlobalState(CACHE_TAG);
    }

    private void onActiveTabChanged(boolean isModelOtr, @Nullable Tab tab) {
        if (tab != null) {
            saveActiveTab(tab);
        } else {
            clearActiveTab(isModelOtr);
        }
    }

    private Callback<@Nullable Tab> getActiveTabChangedCallback(boolean incognito) {
        return incognito ? mOnIncognitoActiveTabChanged : mOnRegularActiveTabChanged;
    }
}
