// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * The correct version of {@link TinkerTankDelegateImpl} will be determined at compile time via
 * build rules.
 */
public interface TinkerTankDelegate {
    public void maybeShowBottomSheet(
            Activity activity,
            Profile profile,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier);

    public void maybeShowForSelectedTabs(
            Activity activity, BottomSheetController bottomSheetController, List<Tab> tabs);
}
