// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Responsible for holding properties of the toolbar in the hub. */
class HubToolbarProperties {
    // When set then an interactable button for the primary pane action should be shown.
    public static final WritableObjectPropertyKey<FullButtonData> ACTION_BUTTON_DATA =
            new WritableObjectPropertyKey();
    // Could be done by setting ACTION_BUTTON_DATA, but a separate property dedupes nicely.
    public static final WritableBooleanPropertyKey SHOW_ACTION_BUTTON_TEXT =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<List<FullButtonData>> PANE_SWITCHER_BUTTON_DATA =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey PANE_SWITCHER_INDEX = new WritableIntPropertyKey();

    // Hold a value from @HubColorScheme.
    public static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey MENU_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SEARCH_BOX_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Runnable> SEARCH_BOX_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();

    @FunctionalInterface
    public interface PaneButtonLookup {
        View get(int index);
    }

    public static final WritableObjectPropertyKey<Callback<PaneButtonLookup>>
            PANE_BUTTON_LOOKUP_CALLBACK = new WritableObjectPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        ACTION_BUTTON_DATA,
        SHOW_ACTION_BUTTON_TEXT,
        PANE_SWITCHER_BUTTON_DATA,
        PANE_SWITCHER_INDEX,
        COLOR_SCHEME,
        MENU_BUTTON_VISIBLE,
        PANE_BUTTON_LOOKUP_CALLBACK,
        SEARCH_BOX_VISIBLE,
        SEARCH_BOX_LISTENER,
        IS_INCOGNITO,
    };
}
