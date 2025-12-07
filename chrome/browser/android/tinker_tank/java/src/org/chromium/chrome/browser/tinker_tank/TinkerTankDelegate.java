// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;
import java.util.function.Supplier;

/** Interface for working with TinkerTank. */
@NullMarked
public interface TinkerTankDelegate {
    private static @Nullable TinkerTankDelegate maybeCreate() {
        return ServiceLoaderUtil.maybeCreate(TinkerTankDelegate.class);
    }

    static TinkerTankDelegate create() {
        TinkerTankDelegate ret = maybeCreate();
        assert ret != null;
        return ret;
    }

    static boolean isEnabled() {
        return maybeCreate() != null
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TINKER_TANK_BOTTOM_SHEET);
    }

    default void maybeShowBottomSheet(
            Activity activity,
            Profile profile,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {}

    void maybeShowForSelectedTabs(
            Activity activity, BottomSheetController bottomSheetController, List<Tab> tabs);
}
