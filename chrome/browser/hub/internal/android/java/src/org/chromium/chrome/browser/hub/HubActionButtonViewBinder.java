// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Applies properties to the action button view in the Hub toolbar. */
@NullMarked
public class HubActionButtonViewBinder {
    public static void bind(PropertyModel model, HubActionButtonView view, PropertyKey key) {
        if (key == ACTION_BUTTON_DATA) {
            setButtonDataByVisibility(model, view);
        } else if (key == COLOR_MIXER) {
            view.setColorMixer(model.get(COLOR_MIXER));
        } else if (key == ACTION_BUTTON_VISIBLE) {
            setButtonDataByVisibility(model, view);
        }
    }

    /** Sets button data based on visibility state. */
    private static void setButtonDataByVisibility(PropertyModel model, HubActionButtonView view) {
        if (model.get(ACTION_BUTTON_VISIBLE)) {
            view.setButtonData(model.get(ACTION_BUTTON_DATA));
        } else {
            view.setButtonData(null);
        }
    }
}
