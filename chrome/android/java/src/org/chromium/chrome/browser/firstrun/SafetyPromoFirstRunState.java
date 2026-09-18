// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_promo.SafetyPromoItem;

/**
 * Carries the card the user picked on the Safety FRE overview page to the carousel page. A null
 * selection means no card is picked, in which case the carousel page is not shown.
 */
@NullMarked
public class SafetyPromoFirstRunState {
    private final SettableNullableObservableSupplier<SafetyPromoItem> mSelectedItemSupplier =
            ObservableSuppliers.createNullable();

    public NullableObservableSupplier<SafetyPromoItem> getSelectedItemSupplier() {
        return mSelectedItemSupplier;
    }

    public void setSelectedItem(@Nullable SafetyPromoItem item) {
        mSelectedItemSupplier.set(item);
    }

    public boolean hasSelectedItem() {
        return mSelectedItemSupplier.get() != null;
    }
}
