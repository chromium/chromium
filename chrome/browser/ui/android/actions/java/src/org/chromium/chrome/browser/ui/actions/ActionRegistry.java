// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.util.SparseArray;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.button.ActionButtonData;

/**
 * A central registry for {@link ActionButtonData}, to register actions that components (like the
 * toolbar, bottom bar, etc.) can render as buttons.
 */
@NullMarked
public class ActionRegistry {
    private final SparseArray<SettableNullableObservableSupplier<ActionButtonData>> mSuppliers =
            new SparseArray<>();

    /**
     * Returns a supplier that consumers can observe to get the action data for a given action ID.
     *
     * @param actionId The ID of the action.
     */
    public NullableObservableSupplier<ActionButtonData> get(@ActionId int actionId) {
        return getOrCreateSupplier(actionId);
    }

    /**
     * Registers or updates the button data for an action.
     *
     * @param actionId The ID of the action.
     * @param data The button data to register or update.
     */
    public void register(@ActionId int actionId, ActionButtonData data) {
        getOrCreateSupplier(actionId).set(data);
    }

    /**
     * Unregisters an action, notifying consumers that it is no longer available.
     *
     * @param actionId The ID of the action to unregister.
     */
    public void unregister(@ActionId int actionId) {
        SettableNullableObservableSupplier<ActionButtonData> supplier = mSuppliers.get(actionId);
        if (supplier != null) {
            supplier.set(null);
        }
    }

    private SettableNullableObservableSupplier<ActionButtonData> getOrCreateSupplier(
            @ActionId int actionId) {
        SettableNullableObservableSupplier<ActionButtonData> supplier = mSuppliers.get(actionId);
        if (supplier == null) {
            supplier = ObservableSuppliers.createNullable();
            mSuppliers.put(actionId, supplier);
        }
        return supplier;
    }
}
