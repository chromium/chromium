// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubSearchBoxBackgroundProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubSearchBoxBackgroundProperties.SHOW_BACKGROUND;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Forwards changed property values to the view. */
public class HubSearchBoxBackgroundViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SHOW_BACKGROUND) {
            view.setVisibility(model.get(SHOW_BACKGROUND) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == COLOR_SCHEME) {
            view.setBackgroundColor(
                    HubColors.getBackgroundColor(view.getContext(), model.get(COLOR_SCHEME)));
        }
    }
}
