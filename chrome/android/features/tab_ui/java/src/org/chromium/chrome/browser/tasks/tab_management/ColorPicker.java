// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.supplier.ObservableSupplier;

/** Interface for the color picker related UI. */
public interface ColorPicker {
    /** Return the inflated container view for the color picker component. */
    ColorPickerContainer getContainerView();

    /**
     * Set a selected color item. No item will be selected by default, so it is recommended to call
     * this after instantiation.
     *
     * @param selectedColor The color id of the color item to be selected.
     */
    void setSelectedColorItem(int selectedColor);

    /** Return the selected color item supplier from the color palette. */
    ObservableSupplier<Integer> getSelectedColorSupplier();
}
