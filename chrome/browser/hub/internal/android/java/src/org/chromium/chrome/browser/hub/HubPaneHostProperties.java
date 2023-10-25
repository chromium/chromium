// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
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

    static final PropertyKey[] ALL_KEYS = {PANE_ROOT_VIEW, ACTION_BUTTON_DATA};
}
