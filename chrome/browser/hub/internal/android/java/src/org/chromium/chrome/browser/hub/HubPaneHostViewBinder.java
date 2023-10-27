// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the view that holds one pane at a time. */
public class HubPaneHostViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, HubPaneHostView view, PropertyKey key) {
        if (key == PANE_ROOT_VIEW) {
            view.setRootView(model.get(PANE_ROOT_VIEW));
        } else if (key == ACTION_BUTTON_DATA) {
            view.setActionButtonData(model.get(ACTION_BUTTON_DATA));
        }
    }
}
