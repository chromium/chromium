// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.util.SparseArray;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.data.CachedProviderAdapter;
import org.chromium.chrome.browser.keyboard_accessory.data.ConditionalProviderAdapter;
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
        AccessoryTabType.PASSWORDS, AccessoryTabType.CREDIT_CARDS, AccessoryTabType.ADDRESSES,
    };
    private final WebContents mWebContents;
    private final SparseArray<SheetState> mSheetStates = new SparseArray<>();
    private final SparseArray<KeyboardAccessoryData.Tab> mAvailableTabs = new SparseArray<>();
    private @Nullable ManualFillingComponent.UpdateAccessorySheetDelegate mUpdater;
    private @Nullable CachedProviderAdapter<KeyboardAccessoryData.Action[]> mActionsProvider;
    private boolean mWebContentsShowing;

    private static class SheetState {
        @Nullable Provider<AccessorySheetData> mDataProvider;

        /**
         *  @deprecated Storing a sheet per WebContents is too expensive. Instead, reuse the already
         *              constructed, browser-scoped sheets in the {@link ManualFillingMediator}!
         *              The state knows about {@link #mAvailableTabs} which is sufficient to request
         *              updates via {@link #requestRecentSheets()} for browser-scoped sheets.
         */
        @Deprecated @Nullable AccessorySheetTabCoordinator mSheet;

        // TODO(crbug.com/1169167): Remove this method when the legacy accessory is cleaned up.
        void notifyProviderObservers() {
            if (mDataProvider instanceof CachedProviderAdapter) {
                ((CachedProviderAdapter<AccessorySheetData>) mDataProvider)
                        .notifyAboutCachedItems();
            }
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
                if (mAvailableTabs.get(state, null) != null) {
                    getStateFor(state).notifyProviderObservers();
                }
            }
        }

        @Override
        public void wasHidden() {
            super.wasHidden();
            mWebContentsShowing = false;
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
     * Repeats the latest data that known {@link CachedProviderAdapter}s cached to all
     * {@link Provider.Observer}s.
     */
    void notifyObservers() {
        if (mActionsProvider != null) mActionsProvider.notifyAboutCachedItems();
        for (int state : TAB_ORDER) {
            // TODO(fhorschig): This needs controller tests for each state in the order!
            if (mAvailableTabs.get(state, null) != null) {
                getStateFor(state).notifyProviderObservers();
            }
        }
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
     * Wraps the given provider for sheet data in a {@link CachedProviderAdapter} and stores it.
     * @param provider A {@link PropertyProvider} providing sheet data.
     */
    void wrapSheetDataProvider(
            @AccessoryTabType int tabType, PropertyProvider<AccessorySheetData> provider) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            // Don't use caching when the new keyboard accessory is enabled.
            getStateFor(tabType).mDataProvider =
                    new ConditionalProviderAdapter<>(provider, () -> mWebContentsShowing);
            return;
        }
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

    /**
     *  @deprecated Storing a sheet per WebContents is too expensive. Reuse the already constructed,
     *              browser-scoped sheets in the {@link ManualFillingMediator} instead!
     */
    @Deprecated
    void setAccessorySheet(
            @AccessoryTabType int tabType, @Nullable AccessorySheetTabCoordinator sheet) {
        assert !ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                : "Storing sheets in a WebContents-scoped cache is too expensive!";
        getStateFor(tabType).mSheet = sheet;
    }

    /**
     * Makes a tab available to the state. If there is already a tab of the same state, this fails.
     * @param tab The @{@link KeyboardAccessoryData.Tab} to track.
     * @return True iff the tab was added. False if the a tab of that type was already added.
     */
    boolean addAvailableTab(KeyboardAccessoryData.Tab tab) {
        if (mAvailableTabs.get(tab.getRecordingType(), null) != null) return false;
        mAvailableTabs.put(tab.getRecordingType(), tab);
        return true;
    }

    /**
     *  @deprecated Storing a sheet per WebContents is too expensive. Reuse the already constructed,
     *              browser-scoped sheets in the {@link ManualFillingMediator} instead!
     */
    @Deprecated
    @Nullable
    AccessorySheetTabCoordinator getAccessorySheet(@AccessoryTabType int tabType) {
        assert !ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                : "Storing sheets in a WebContents-scoped cache is too expensive!";
        return getStateFor(tabType).mSheet;
    }

    private void onAdapterReceivedNewData(CachedProviderAdapter adapter) {
        if (mWebContentsShowing) adapter.notifyAboutCachedItems();
    }

    WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
