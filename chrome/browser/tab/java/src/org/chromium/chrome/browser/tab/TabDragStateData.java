// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Tracks whether a tab is being dragged. */
@NullMarked
public class TabDragStateData implements UserData {
    private final SettableNonNullObservableSupplier<Boolean> mIsDraggingSupplier =
            ObservableSuppliers.createNonNull(false);

    private TabDragStateData() {}

    /** Returns the {@link TabDragStateData} for the {@link Tab} or null. */
    public static @Nullable TabDragStateData getForTab(Tab tab) {
        if (tab == null || tab.getUserDataHost() == null) return null;
        return tab.getUserDataHost().getUserData(TabDragStateData.class);
    }

    /** Returns the {@link TabDragStateData} for the {@link Tab} creating it if needed. */
    public static TabDragStateData getOrCreateForTab(Tab tab) {
        assert !tab.isDestroyed();
        @Nullable TabDragStateData data = getForTab(tab);
        if (data != null) return data;

        return tab.getUserDataHost().setUserData(TabDragStateData.class, new TabDragStateData());
    }

    /** Sets whether the tab is being dragged. */
    public void setIsDragging(boolean isDragging) {
        mIsDraggingSupplier.set(isDragging);
    }

    /** Returns the supplier for the drag state. */
    public NonNullObservableSupplier<Boolean> getIsDraggingSupplier() {
        return mIsDraggingSupplier;
    }
}
