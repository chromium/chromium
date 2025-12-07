// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Point;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Returns context menu data for the last context menu shown on the tab. */
@NullMarked
public class TabContextMenuData implements UserData {
    private @Nullable Point mLastTriggeringTouchPositionDp;
    private final ObservableSupplierImpl<Boolean> mTabContextMenuVisibility =
            new ObservableSupplierImpl<>(false);

    private TabContextMenuData() {}

    /** Returns the {@link TabContextMenuData} for the {@link Tab} or null. */
    public static @Nullable TabContextMenuData getForTab(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        return host.getUserData(TabContextMenuData.class);
    }

    /** Returns the {@link TabContextMenuData} for the {@link Tab} creating it if needed. */
    public static TabContextMenuData getOrCreateForTab(Tab tab) {
        assert !tab.isDestroyed();
        @Nullable TabContextMenuData data = getForTab(tab);
        return data != null
                ? data
                : tab.getUserDataHost()
                        .setUserData(TabContextMenuData.class, new TabContextMenuData());
    }

    /**
     * Sets the last triggering touch position in dp, may be null if the coordinates should be
     * cleared.
     */
    public void setLastTriggeringTouchPositionDp(@Nullable Point point) {
        mLastTriggeringTouchPositionDp = point;
        mTabContextMenuVisibility.set(point != null);
    }

    /** Sets the last triggering touch position in dp. */
    public void setLastTriggeringTouchPositionDp(int x, int y) {
        setLastTriggeringTouchPositionDp(new Point(x, y));
    }

    /**
     * Returns the triggering touch position the last time the context menu was shown or null if it
     * has been cleared.
     */
    public @Nullable Point getLastTriggeringTouchPositionDp() {
        return mLastTriggeringTouchPositionDp;
    }

    /** Returns the supplier for the context menu visibility. */
    public ObservableSupplierImpl<Boolean> getTabContextMenuVisibilitySupplier() {
        return mTabContextMenuVisibility;
    }
}
