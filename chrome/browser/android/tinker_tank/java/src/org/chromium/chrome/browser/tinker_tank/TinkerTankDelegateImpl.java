// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Upstream implementation of {@link TinkerTankDelegate}. Downstream targets may provide a different
 * implementation.
 */
public class TinkerTankDelegateImpl implements TinkerTankDelegate {
    @Override
    public boolean isEnabled() {
        return false;
    }

    @Override
    public void maybeShowBottomSheet(
            Activity activity,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {}
}
