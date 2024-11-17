// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.util.SparseArray;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.data.CachedProviderAdapter;
import org.chromium.chrome.browser.keyboard_accessory.data.ConditionalProviderAdapter;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.ArrayList;

/**
 * This class holds all data that is necessary to restore the state of the Keyboard accessory
 * and its sheet for the {@link WebContents} it is attached to.
 */
class ManualFillingState {
    private static final int[] TAB_ORDER = {
        AccessoryTabType.PASSWORDS, AccessoryTabType.CREDIT_CARDS, AccessoryTabType.ADDRESSES,
    };
    private final WebContents mWebContents;
    private final SparseArray<Provider<AccessorySheetData>> mSheetDataProviders =
            new SparseArray<>();
    private final SparseArray<KeyboardAccessoryData.Tab> mAvailableTabs = new SparseArray<>();
    private @Nullable ManualFillingComponent.UpdateAccessorySheetDelegate mUpdater;
    private @Nullable CachedProviderAdapter<KeyboardAccessoryData.Action[]> mActionsProvider;
    private boolean mWebContentsShowing;

    private class Observer extends WebContentsObserver {
        public Observer(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void onVisibilityChanged(@Visibility int visibility) {
            mWebContentsShowing = visibility == Visibility.VISIBLE;
            if (mWebContentsShowing && mActionsProvider != null) {
                mActionsProvider.notifyAboutCachedItems();
            }
        }
    }

    private final WebContentsObserver mWebContentsObserver;

    /**
     * Creates a new set of user data that is bound to and observing the given web contents.
     * @param webContents Some {@link WebContents} which are assumed to be shown right now.
     */
    ManualFillingState(@Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            mWebContents = null;
            mWebContentsObserver = null;
            return;
        }
        mWebContents = webContents;
        mWebContentsShowing = true;
        mWebContentsObserver = new Observer(mWebContents);
        mWebContents.addObserver(mWebContentsObserver);
    }

    /**
     * Repeats the latest data that known {@link CachedProviderAdapter}s cached to all {@link
     * Provider.Observer}s.
     */
    void notifyObservers() {
        if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
    }

    void setSheetUpdater(ManualFillingComponent.UpdateAccessorySheetDelegate delegate) {
        mUpdater = delegate;
    }

    void requestRecentSheets() {
        for (@AccessoryTabType int type : TAB_ORDER) {
            if (mAvailableTabs.get(type, null) != null && mUpdater != null) {
                mUpdater.requestSheet(type);
            }
        }
    }

    KeyboardAccessoryData.Tab[] getTabs() {
        ArrayList<KeyboardAccessoryData.Tab> tabs = new ArrayList<>();
        for (@AccessoryTabType int type : TAB_ORDER) {
            KeyboardAccessoryData.Tab tab = mAvailableTabs.get(type, null);
            if (tab != null) tabs.add(mAvailableTabs.get(type));
        }
        return tabs.toArray(new KeyboardAccessoryData.Tab[0]);
    }

    void destroy() {
        if (mWebContents != null) mWebContents.removeObserver(mWebContentsObserver);
        mActionsProvider = null;
        mSheetDataProviders.clear();
        mWebContentsShowing = false;
    }

    /**
     * Wraps the given ActionProvider in a {@link CachedProviderAdapter} and stores it.
     *
     * @param provider A {@link PropertyProvider} providing actions.
     * @param defaultActions A default set of actions to prepopulate the adapter's cache.
     */
    void wrapActionsProvider(
            PropertyProvider<KeyboardAccessoryData.Action[]> provider,
            KeyboardAccessoryData.Action[] defaultActions) {
        mActionsProvider =
                new CachedProviderAdapter<>(
                        provider, defaultActions, this::onAdapterReceivedNewData);
    }

    /**
     * Returns the wrapped provider set with {@link #wrapActionsProvider}.
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    Provider<KeyboardAccessoryData.Action[]> getActionsProvider() {
        return mActionsProvider;
    }

    /**
     * Wraps the given provider for sheet data in a {@link ConditionalProviderAdapter} and stores
     * it.
     *
     * @param provider A {@link PropertyProvider} providing sheet data.
     */
    void wrapSheetDataProvider(
            @AccessoryTabType int tabType, PropertyProvider<AccessorySheetData> provider) {
        mSheetDataProviders.put(
                tabType, new ConditionalProviderAdapter<>(provider, () -> mWebContentsShowing));
    }

    /**
     * Returns the wrapped provider set with {@link #wrapSheetDataProvider}.
     *
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    @Nullable
    Provider<AccessorySheetData> getSheetDataProvider(@AccessoryTabType int tabType) {
        return mSheetDataProviders.get(tabType);
    }

    /**
     * Makes a tab available to the state. If there is already a tab of the same state, this fails.
     *
     * @param tab The @{@link KeyboardAccessoryData.Tab} to track.
     * @return True iff the tab was added. False if the a tab of that type was already added.
     */
    boolean addAvailableTab(KeyboardAccessoryData.Tab tab) {
        if (mAvailableTabs.get(tab.getRecordingType(), null) != null) return false;
        mAvailableTabs.put(tab.getRecordingType(), tab);
        return true;
    }

    private void onAdapterReceivedNewData(CachedProviderAdapter adapter) {
        if (mWebContentsShowing) adapter.notifyAboutCachedItems();
    }

    WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
