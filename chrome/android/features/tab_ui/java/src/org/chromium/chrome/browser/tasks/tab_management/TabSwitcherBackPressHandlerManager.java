// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.ArrayList;

/**
 * Central manager for handling BackPress-related business of {@link TabSwitcherDragHandler}
 * instances.
 *
 * <p>This class implements {@link BackPressHandler} to provide a unified escape press handler for
 * all TabSwitcherDragHandler(also implements BackPressHandler) instances. It aggregates the state
 * of all TabSwitcherDragHandlers; if any registered handler indicates a drag is in progress, this
 * manager reports itself as active to the {@link BackPressManager}.
 *
 * <p>When an ESC key press is intercepted, this manager delegates the handling logic to the
 * specific child handler that is currently active.
 */
@NullMarked
public class TabSwitcherBackPressHandlerManager implements BackPressHandler {

    private final ArrayList<TabSwitcherDragHandler> mHandlers =
            new ArrayList<TabSwitcherDragHandler>();
    private final SettableNonNullObservableSupplier<Boolean> mDragInProgressSupplier =
            ObservableSuppliers.createNonNull(false);

    public TabSwitcherBackPressHandlerManager() {}

    /**
     * Registers a {@link TabSwitcherDragHandler} to be managed.
     *
     * <p>The manager will observe the TabSwitcherDragHandler's dragging state.
     *
     * @param handler The drag handler to add.
     */
    public void addHandler(TabSwitcherDragHandler handler) {
        mHandlers.add(handler);
        Callback<Boolean> observerCallback = (t) -> onStatusChanged();
        handler.getHandleBackPressChangedSupplier()
                .addSyncObserverAndPostIfNonNull(observerCallback);
    }

    public void removeHandler(TabSwitcherDragHandler handler) {
        mHandlers.remove(handler);
        onStatusChanged();
    }

    private void onStatusChanged() {
        for (TabSwitcherDragHandler handler : mHandlers) {
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled) {
                mDragInProgressSupplier.set(true);
                return;
            }
        }
        mDragInProgressSupplier.set(false);
    }

    @Override
    public boolean invokeBackActionOnEscape() {
        return false;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mDragInProgressSupplier;
    }

    @Override
    public Boolean handleEscPress() {
        Boolean res = false;
        for (TabSwitcherDragHandler handler : mHandlers) {
            if (handler.getHandleBackPressChangedSupplier().get()) {
                res |= handler.handleEscPress();
            }
        }
        return res;
    }
}
