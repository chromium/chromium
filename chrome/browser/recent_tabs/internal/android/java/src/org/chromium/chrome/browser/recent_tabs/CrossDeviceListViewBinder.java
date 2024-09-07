// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.chrome.browser.recent_tabs.CrossDeviceListProperties.EMPTY_STATE_VISIBLE;

import org.chromium.chrome.browser.recent_tabs.ui.CrossDevicePaneView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Forwards changed property values to the view. */
public class CrossDeviceListViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(
            PropertyModel model, CrossDevicePaneView view, PropertyKey propertyKey) {
        if (propertyKey == EMPTY_STATE_VISIBLE) {
            view.setEmptyStateVisible(model.get(EMPTY_STATE_VISIBLE));
        }
    }
}
