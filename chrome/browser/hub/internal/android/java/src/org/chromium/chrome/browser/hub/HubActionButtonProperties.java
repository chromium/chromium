// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for holding properties of the action button in the Hub toolbar. */
@NullMarked
public class HubActionButtonProperties {
    // When set then an interactable button for the primary pane action should be shown.
    public static final WritableObjectPropertyKey<FullButtonData> ACTION_BUTTON_DATA =
            new WritableObjectPropertyKey();

    // The visibility of the action button.
    public static final WritableBooleanPropertyKey ACTION_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    // All property keys for the action button.
    static final PropertyKey[] ALL_ACTION_BUTTON_KEYS = {
        ACTION_BUTTON_DATA, COLOR_MIXER, ACTION_BUTTON_VISIBLE,
    };
}
