// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the view that holds one pane at a time. */
public class HubPaneHostViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, HubPaneHostView view, PropertyKey key) {
        if (key == PANE_ROOT_VIEW) {
            view.setRootView(model.get(PANE_ROOT_VIEW));
        } else if (key == COLOR_SCHEME) {
            view.setColorScheme(model.get(COLOR_SCHEME));
        } else if (key == HAIRLINE_VISIBILITY) {
            view.setHairlineVisibility(model.get(HAIRLINE_VISIBILITY));
        } else if (key == SNACKBAR_CONTAINER_CALLBACK) {
            view.setSnackbarContainerConsumer(model.get(SNACKBAR_CONTAINER_CALLBACK));
        }
    }
}
