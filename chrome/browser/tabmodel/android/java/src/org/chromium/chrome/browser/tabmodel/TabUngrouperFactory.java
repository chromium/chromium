// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/** Creates a {@link TabUngrouper} for {@link TabGroupModelFilterFactory}. */
@FunctionalInterface
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public interface TabUngrouperFactory {
    /**
     * @param isIncognitoBranded Whether the filter is for incognito tabs.
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     * @return a {@link TabUngrouper}.
     */
    /*package*/
    TabUngrouper create(
            boolean isIncognitoBranded,
            Supplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier);
}
