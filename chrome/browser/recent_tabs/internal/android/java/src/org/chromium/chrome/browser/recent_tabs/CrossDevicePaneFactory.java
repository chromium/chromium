// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;

import java.util.function.DoubleConsumer;

/** A factory interface for building a {@link Pane} instance. */
@NullMarked
public class CrossDevicePaneFactory {
    /**
     * Create an instance of the {@link Pane}.
     *
     * @param context Used to inflate UI.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    public static Pane create(
            Context context,
            DoubleConsumer onToolbarAlphaChange,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        return new CrossDevicePaneImpl(context, onToolbarAlphaChange, edgeToEdgeSupplier);
    }
}
