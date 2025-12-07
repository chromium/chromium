// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for applying properties to the bottom toolbar in the hub. */
@NullMarked
public class HubBottomToolbarViewBinder {
    public static void bind(PropertyModel model, HubBottomToolbarView view, PropertyKey key) {
        if (key == COLOR_MIXER) {
            view.setColorMixer(model.get(COLOR_MIXER));
        } else if (key == BOTTOM_TOOLBAR_VISIBLE) {
            boolean isVisible = model.get(BOTTOM_TOOLBAR_VISIBLE);
            view.setVisibility(isVisible ? VISIBLE : GONE);
        }
    }
}
