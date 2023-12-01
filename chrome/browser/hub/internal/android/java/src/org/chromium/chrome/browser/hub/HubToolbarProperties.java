// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Responsible for holding properties of the toolbar in the hub. */
class HubToolbarProperties {
    /** When set then an interactable button for the primary pane action should be shown. */
    public static final WritableObjectPropertyKey<FullButtonData> ACTION_BUTTON_DATA =
            new WritableObjectPropertyKey();

    /** Could be done by setting ACTION_BUTTON_DATA, but a separate property dedupes nicely. */
    public static final WritableBooleanPropertyKey SHOW_ACTION_BUTTON_TEXT =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<List<FullButtonData>> PANE_SWITCHER_BUTTON_DATA =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        ACTION_BUTTON_DATA, SHOW_ACTION_BUTTON_TEXT, PANE_SWITCHER_BUTTON_DATA
    };
}
