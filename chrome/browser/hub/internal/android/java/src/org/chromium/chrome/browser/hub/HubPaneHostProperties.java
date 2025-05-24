// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for holding properties of hub pane host views. */
@NullMarked
class HubPaneHostProperties {
    /**
     * The root view of the pane that should be inserted into view hierarchy. When changed, the
     * previous value should be removed.
     */
    public static final WritableObjectPropertyKey<View> PANE_ROOT_VIEW =
            new WritableObjectPropertyKey();

    public static final WritableBooleanPropertyKey HAIRLINE_VISIBILITY =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Callback<ViewGroup>> SNACKBAR_CONTAINER_CALLBACK =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        PANE_ROOT_VIEW, COLOR_MIXER, HAIRLINE_VISIBILITY, SNACKBAR_CONTAINER_CALLBACK,
    };
}
