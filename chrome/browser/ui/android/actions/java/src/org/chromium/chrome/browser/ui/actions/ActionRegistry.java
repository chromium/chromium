// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.util.SparseArray;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A central registry for {@link ActionId}, to register actions that components (like the toolbar,
 * bottom bar, etc.) can render as buttons.
 */
@NullMarked
public class ActionRegistry {
    private final SparseArray<SettableNullableObservableSupplier<PropertyModel>> mSuppliers =
            new SparseArray<>();

    /**
     * Returns an ObservableSupplier for the PropertyModel that consumers can observe for changes to
     * the given ActionId.
     */
    public NullableObservableSupplier<PropertyModel> get(@ActionId int id) {
        return getSupplierForId(id);
    }

    /**
     * Registers a PropertyModel for a specific ActionId, broadcasting it to any surfaces that are
     * already listening for this ID.
     */
    public void register(@ActionId int id, PropertyModel model) {
        getSupplierForId(id).set(model);
    }

    /**
     * Unregisters a PropertyModel for a specific ActionId, notifying consumers that it is no longer
     * available.
     */
    public void unregister(@ActionId int id) {
        SettableNullableObservableSupplier<PropertyModel> supplier = mSuppliers.get(id);
        if (supplier != null) {
            supplier.set(null);
        }
    }

    private SettableNullableObservableSupplier<PropertyModel> getSupplierForId(@ActionId int id) {
        SettableNullableObservableSupplier<PropertyModel> supplier = mSuppliers.get(id);
        if (supplier == null) {
            supplier = ObservableSuppliers.createNullable();
            mSuppliers.put(id, supplier);
        }
        return supplier;
    }
}
