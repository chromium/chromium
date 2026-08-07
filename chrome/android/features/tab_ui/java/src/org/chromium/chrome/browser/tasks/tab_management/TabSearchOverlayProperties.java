// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Tab Search Overlay component. */
@NullMarked
public class TabSearchOverlayProperties {
    /** Controls the visibility of the empty state view inside the overlay panel. */
    public static final WritableBooleanPropertyKey EMPTY_STATE_VISIBLE =
            new WritableBooleanPropertyKey("empty_state_visible");

    /** Click listener for the close button to dismiss the overlay. */
    public static final WritableObjectPropertyKey<OnClickListener> ON_CLOSE_CLICK =
            new WritableObjectPropertyKey<>("on_close_click");

    /** Callback executed when the overlay's hide animation completes. */
    public static final WritableObjectPropertyKey<Runnable> ON_HIDE_FINISHED =
            new WritableObjectPropertyKey<>("on_hide_finished");

    /** Click listener for the background scrim view to dismiss the overlay. */
    public static final WritableObjectPropertyKey<OnClickListener> ON_SCRIM_CLICK =
            new WritableObjectPropertyKey<>("on_scrim_click");

    /** Controls the visibility of the overlay panel. */
    public static final WritableBooleanPropertyKey VISIBLE =
            new WritableBooleanPropertyKey("visible");

    /** Controls the UI/behavior of the search panel based on incognito mode. */
    public static final WritableBooleanPropertyKey IS_INCOGNITO =
            new WritableBooleanPropertyKey("is_incognito");

    public static final PropertyKey[] ALL_KEYS = {
        EMPTY_STATE_VISIBLE, ON_CLOSE_CLICK, ON_HIDE_FINISHED, ON_SCRIM_CLICK, VISIBLE, IS_INCOGNITO
    };

    /** Creates a default PropertyModel with all keys. */
    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
