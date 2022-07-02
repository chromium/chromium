// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * A {@link BackPressHandler} intercepting back press when the current selected tab is launched
 * from reading list.
 */
public class ReadingListBackPressHandler implements BackPressHandler, Destroyable {
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<TabModelSelector> mOnTabModelSelectorAvailableCallback;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;

    public ReadingListBackPressHandler(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mOnTabModelSelectorAvailableCallback = this::onTabModelSelectorAvailable;
        mTabModelSelectorSupplier.addObserver(mOnTabModelSelectorAvailableCallback);
    }

    @Override
    public void handleBackPress() {
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        assert selector != null;
        ReadingListUtils.showReadingList(selector.getCurrentTab().isIncognito());
        WebContents webContents = selector.getCurrentTab().getWebContents();
        if (webContents != null) webContents.dispatchBeforeUnload(false);
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        if (mTabModelSelectorTabModelObserver != null) {
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabModelObserver = null;
        }
    }

    private void onTabModelSelectorAvailable(TabModelSelector selector) {
        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                mBackPressChangedSupplier.set(
                        tab.getLaunchType() == TabLaunchType.FROM_READING_LIST);
            }
        };
        mTabModelSelectorSupplier.removeObserver(mOnTabModelSelectorAvailableCallback);
    }
}
