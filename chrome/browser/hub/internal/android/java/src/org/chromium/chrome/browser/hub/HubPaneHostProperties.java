// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for holding properties of hub pane host views. */
class HubPaneHostProperties {
    /**
     * The root view of the pane that should be inserted into view hierarchy. When changed, the
     * previous value should be removed.
     */
    public static final WritableObjectPropertyKey<View> PANE_ROOT_VIEW =
            new WritableObjectPropertyKey();

    /** When set then an interactable button for the primary pane action should be shown. */
    public static final WritableObjectPropertyKey<FullButtonData> ACTION_BUTTON_DATA =
            new WritableObjectPropertyKey();

    // Hold a value from @HubColorScheme.
    public static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey HAIRLINE_VISIBILITY =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Callback<Supplier<View>>>
            FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Callback<ViewGroup>> SNACKBAR_CONTAINER_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey EDGE_TO_EDGE_BOTTOM_INSETS =
            new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        PANE_ROOT_VIEW,
        ACTION_BUTTON_DATA,
        COLOR_SCHEME,
        HAIRLINE_VISIBILITY,
        FLOATING_ACTION_BUTTON_SUPPLIER_CALLBACK,
        SNACKBAR_CONTAINER_CALLBACK,
        EDGE_TO_EDGE_BOTTOM_INSETS
    };
}
