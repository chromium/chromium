// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * Upstream implementation of {@link TinkerTankDelegate}. Downstream targets may provide a different
 * implementation.
 */
public class TinkerTankDelegateImpl implements TinkerTankDelegate {
    public static boolean enabled() {
        return false;
    }

    @Override
    public void maybeShowBottomSheet(
            Activity activity,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {}

    @Override
    public void maybeShowForSelectedTabs(
            Activity activity, BottomSheetController bottomSheetController, List<Tab> tabs) {}
}
