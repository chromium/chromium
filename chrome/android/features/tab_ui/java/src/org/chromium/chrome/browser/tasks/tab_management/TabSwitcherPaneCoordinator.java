// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/** Coordinator for a {@link TabSwitcherPaneBase}'s UI. */
// TODO(crbug/1505772): Implement this class it is currently a bunch of stubs.
public class TabSwitcherPaneCoordinator implements BackPressHandler {

    /**
     * @param parentView The view to use as a parent.
     */
    public TabSwitcherPaneCoordinator(ViewGroup parentView) {}

    /** Destroys the coordinator. */
    public void destroy() {}

    /** Post native initialization. */
    public void initWithNative() {}

    /** Shows the tab list editor. */
    public void showTabListEditor() {
        assert false : "Not implemented.";
    }

    /**
     * Resets the UI with the specified tabs.
     *
     * @param tabList The {@link TabList} to show tabs for.
     */
    public void resetWithTabList(@Nullable TabList tabList) {
        assert false : "Not implemented.";
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        assert false : "Not implemented.";
        return () -> false;
    }

    /** Returns a {@link TabSwitcherCustomViewManager} for supplying custom views. */
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        assert false : "Not implemented.";
        return null;
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    public int getTabSwitcherTabListModelSize() {
        assert false : "Not implemented.";
        return 0;
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        assert false : "Not implemented.";
    }

    @Override
    public @BackPressResult int handleBackPress() {
        assert false : "Not implemented.";
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        assert false : "Not implemented.";
        return new ObservableSupplierImpl<>();
    }
}
