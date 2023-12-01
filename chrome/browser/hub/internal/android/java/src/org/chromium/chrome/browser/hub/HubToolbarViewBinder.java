// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the view that holds one pane at a time. */
public class HubToolbarViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, HubToolbarView view, PropertyKey key) {
        if (key == ACTION_BUTTON_DATA || key == SHOW_ACTION_BUTTON_TEXT) {
            view.setActionButtonData(
                    model.get(ACTION_BUTTON_DATA), model.get(SHOW_ACTION_BUTTON_TEXT));
        }
    }
}
