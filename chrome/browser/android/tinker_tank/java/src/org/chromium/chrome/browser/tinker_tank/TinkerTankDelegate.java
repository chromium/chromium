// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * The correct version of {@link TinkerTankDelegateImpl} will be determined at compile time via
 * build rules.
 */
public interface TinkerTankDelegate {
    public boolean isEnabled();

    public void maybeShowBottomSheet(
            Activity activity,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier);
}
