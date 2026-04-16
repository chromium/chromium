// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties used for the NewTabPageLayout. */
@NullMarked
class NewTabPageLayoutProperties {
    /** The delegate for NewTabPageLayout. */
    static final PropertyModel.WritableObjectPropertyKey<NewTabPageLayout.Delegate> DELEGATE =
            new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * Sets the layout change listener for NewTabPageLayout. Previously added listener will be
     * removed.
     */
    static final PropertyModel.WritableObjectPropertyKey<View.OnLayoutChangeListener>
            ON_LAYOUT_CHANGE_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    /** The system's top inset for the NewTabPageLayout. */
    static final WritableIntPropertyKey TOP_INSET_PX = new WritableIntPropertyKey();

    /**
     * The transition y of the fake search box and all views above it. Used when the url focus
     * animation is combined with the omnibox suggestions list animation to reduce the number of
     * visual elements in motion.
     */
    static final WritableFloatPropertyKey TRANSITION_Y = new WritableFloatPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        DELEGATE, ON_LAYOUT_CHANGE_LISTENER, TOP_INSET_PX, TRANSITION_Y,
    };
}
