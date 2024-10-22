// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;

/** Creates a {@link TabUngrouper} for {@link TabGroupModelFilterFactory}. */
interface TabUngrouperFactory {
    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     * @return a {@link TabUngrouper}.
     */
    /*package*/ @NonNull
    TabUngrouper create(@NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier);
}
