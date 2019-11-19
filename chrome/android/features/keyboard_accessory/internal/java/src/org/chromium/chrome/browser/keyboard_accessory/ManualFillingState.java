// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.util.SparseArray;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.data.CachedProviderAdapter;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabCoordinator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.ArrayList;

/**
 * This class holds all data that is necessary to restore the state of the Keyboard accessory
 * and its sheet for the {@link WebContents} it is attached to.
 */
class ManualFillingState {
    private static final int[] TAB_ORDER = {
            AccessoryTabType.PASSWORDS,
            AccessoryTabType.CREDIT_CARDS,
            AccessoryTabType.ADDRESSES,
            AccessoryTabType.TOUCH_TO_FILL,
    };
    private final WebContents mWebContents;
    private final SparseArray<SheetState> mSheetStates = new SparseArray<>();
    private @Nullable CachedProviderAdapter<KeyboardAccessoryData.Action[]> mActionsProvider;
    private boolean mWebContentsShowing;

    private static class SheetState {
        @Nullable
        CachedProviderAdapter<AccessorySheetData> mDataProvider;
        @Nullable
        AccessorySheetTabCoordinator mSheet;

        void notifyProviderObservers() {
            if (mDataProvider != null) mDataProvider.notifyAboutCachedItems();
        }
    }

    private class Observer extends WebContentsObserver {
        public Observer(WebContents webContents) {
            super(webContents);
        }

        @Override
        public void wasShown() {
            super.wasShown();
            mWebContentsShowing = true;
            if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
            for (int state : TAB_ORDER) {
                getStateFor(state).notifyProviderObservers();
            }
        }

        @Override
        public void wasHidden() {
            super.wasHidden();
            mWebContentsShowing = false;
        }
    };

    private final WebContentsObserver mWebContentsObserver;

    /**
     * Creates a new set of user data that is bound to and observing the given web contents.
     * @param webContents Some {@link WebContents} which are assumed to be shown right now.
     */
    ManualFillingState(@Nullable WebContents webContents) {
        mWebContents = webContents;
        if (webContents == null) {
            mWebContentsObserver = null;
            return;
        }
        mWebContentsShowing = true;
        mWebContentsObserver = new Observer(mWebContents);
        mWebContents.addObserver(mWebContentsObserver);
    }

    /**
     * Repeats the latest data that known {@link CachedProviderAdapter}s cached to all
     * {@link Provider.Observer}s.
     */
    void notifyObservers() {
        if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
        for (int state : TAB_ORDER) {
            // TODO(fhorschig): This needs controller tests for each state in the order!
            getStateFor(state).notifyProviderObservers();
        }
    }

    KeyboardAccessoryData.Tab[] getTabs() {
        ArrayList<KeyboardAccessoryData.Tab> tabs = new ArrayList<>();
        for (@AccessoryTabType int type : TAB_ORDER) {
            SheetState state = getStateFor(type);
            if (state.mSheet != null) tabs.add(state.mSheet.getTab());
        }
        return tabs.toArray(new KeyboardAccessoryData.Tab[0]);
    }

    void destroy() {
        if (mWebContents != null) mWebContents.removeObserver(mWebContentsObserver);
        mActionsProvider = null;
        mSheetStates.clear();
        mWebContentsShowing = false;
    }

    private SheetState getStateFor(@AccessoryTabType int tabType) {
        SheetState state = mSheetStates.get(tabType);
        if (state == null) {
            mSheetStates.put(tabType, new SheetState());
            state = mSheetStates.get(tabType);
        }
        return state;
    }

    /**
     * Wraps the given ActionProvider in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing actions.
     * @param defaultActions A default set of actions to prepopulate the adapter's cache.
     */
    void wrapActionsProvider(PropertyProvider<KeyboardAccessoryData.Action[]> provider,
            KeyboardAccessoryData.Action[] defaultActions) {
        mActionsProvider = new CachedProviderAdapter<>(
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
     * Wraps the given provider for sheet data in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing sheet data.
     */
    void wrapSheetDataProvider(
            @AccessoryTabType int tabType, PropertyProvider<AccessorySheetData> provider) {
        getStateFor(tabType).mDataProvider =
                new CachedProviderAdapter<>(provider, null, this::onAdapterReceivedNewData);
    }

    /**
     * Returns the wrapped provider set with {@link #wrapSheetDataProvider}.
     * @return A {@link CachedProviderAdapter} wrapping a {@link PropertyProvider}.
     */
    Provider<AccessorySheetData> getSheetDataProvider(@AccessoryTabType int tabType) {
        return getStateFor(tabType).mDataProvider;
    }

    void setAccessorySheet(
            @AccessoryTabType int tabType, @Nullable AccessorySheetTabCoordinator sheet) {
        getStateFor(tabType).mSheet = sheet;
    }

    @Nullable
    AccessorySheetTabCoordinator getAccessorySheet(@AccessoryTabType int tabType) {
        return getStateFor(tabType).mSheet;
    }

    private void onAdapterReceivedNewData(CachedProviderAdapter adapter) {
        if (mWebContentsShowing) adapter.notifyAboutCachedItems();
    }

    @VisibleForTesting
    WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
