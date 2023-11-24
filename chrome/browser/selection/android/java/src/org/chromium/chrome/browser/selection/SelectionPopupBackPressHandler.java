// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.SelectionPopupController;

/**
 * {@link BackPressHandler} of {@link SelectionPopupController}. This listens to the change of tab
 * model and notifies whether the current selection popup controller is going to intercept the
 * back press.
 */
public class SelectionPopupBackPressHandler extends EmptyTabObserver
        implements BackPressHandler, TabModelObserver, Destroyable {
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Boolean> mCallback = this::onActionBarShowingChanged;

    private SelectionPopupController mPopupController;
    private Tab mTab;

    /**
     * @param tabModelSelector A {@link TabModelSelector} which can provide
     * {@link org.chromium.chrome.browser.tabmodel.TabModelFilterProvider}.
     */
    public SelectionPopupBackPressHandler(TabModelSelector tabModelSelector) {
        tabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(this);
    }

    @Override
    public @BackPressResult int handleBackPress() {
        assert mPopupController != null;
        int res =
                mPopupController.isSelectActionBarShowing()
                        ? BackPressResult.SUCCESS
                        : BackPressResult.FAILURE;
        mPopupController.clearSelection();
        return res;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        mBackPressChangedSupplier.set(false);
        updatePopupControllerObserving(tab);
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
        updatePopupControllerObserving(tab);
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePopupControllerObserving(tab);
    }

    @Override
    public void destroy() {
        updatePopupControllerObserving(null);
        mBackPressChangedSupplier.set(false);
    }

    private void updatePopupControllerObserving(Tab tab) {
        if (mPopupController != null) {
            mPopupController.isSelectActionBarShowingSupplier().removeObserver(mCallback);
            mPopupController = null;
        }
        if (mTab != null) mTab.removeObserver(this);
        mTab = tab;
        if (tab == null) return;
        var webContents = tab.getWebContents();
        if (webContents == null) return;
        tab.addObserver(this);
        mPopupController = SelectionPopupController.fromWebContents(webContents);
        mPopupController.isSelectActionBarShowingSupplier().addObserver(mCallback);
    }

    private void onActionBarShowingChanged(boolean isShowing) {
        mBackPressChangedSupplier.set(isShowing);
    }

    SelectionPopupController getPopupControllerForTesting() {
        return mPopupController;
    }
}
