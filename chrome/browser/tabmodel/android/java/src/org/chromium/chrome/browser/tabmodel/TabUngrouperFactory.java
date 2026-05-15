// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/** Creates a {@link TabUngrouper}. */
@FunctionalInterface
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
@NullMarked
public interface TabUngrouperFactory {
    /**
     * @param isIncognitoBranded Whether the model is for incognito tabs.
     * @param tabModelSupplier The supplier of the {@link TabModel}.
     * @return a {@link TabUngrouper}.
     */
    /*package*/
    TabUngrouper create(boolean isIncognitoBranded, Supplier<@Nullable TabModel> tabModelSupplier);
}
