// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tinker_tank;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;
import java.util.function.Supplier;

/** Interface for working with TinkerTank. */
@Deprecated
@NullMarked
public interface TinkerTankDelegate {
    default void maybeShowBottomSheet(
            Activity activity,
            Profile profile,
            BottomSheetController bottomSheetController,
            Supplier<TabModelSelector> tabModelSelectorSupplier) {}

    default void maybeShowForSelectedTabs(
            Activity activity, BottomSheetController bottomSheetController, List<Tab> tabs) {}
}
