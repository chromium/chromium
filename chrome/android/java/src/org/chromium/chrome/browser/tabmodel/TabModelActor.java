// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;

/** Base class for performing tab model actions. */
public abstract class TabModelActor {
    private final Supplier<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    /**
     * @param tabGroupModelFilterSupplier The supplier of the {@link TabGroupModelFilter}.
     */
    public TabModelActor(@NonNull Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
    }

    protected @NonNull TabGroupModelFilter getTabGroupModelFilter() {
        assert mTabGroupModelFilterSupplier.hasValue();
        return mTabGroupModelFilterSupplier.get();
    }
}
