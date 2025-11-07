// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** Responsible for holding properties of the toolbar in the hub. */
@NullMarked
class HubToolbarProperties {
    public static final WritableObjectPropertyKey<List<FullButtonData>> PANE_SWITCHER_BUTTON_DATA =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey PANE_SWITCHER_INDEX = new WritableIntPropertyKey();

    public static final WritableBooleanPropertyKey MENU_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SEARCH_BOX_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey SEARCH_LOUPE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey HUB_SEARCH_ENABLED_STATE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Runnable> SEARCH_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey BACK_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey BACK_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<Runnable> BACK_BUTTON_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION =
            new WritableBooleanPropertyKey();

    public static final WritableBooleanPropertyKey HAIRLINE_VISIBILITY =
            new WritableBooleanPropertyKey();

    @FunctionalInterface
    public interface PaneButtonLookup {
        @Nullable View get(int index);
    }

    public static final WritableObjectPropertyKey<Callback<PaneButtonLookup>>
            PANE_BUTTON_LOOKUP_CALLBACK = new WritableObjectPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        PANE_SWITCHER_BUTTON_DATA,
        PANE_SWITCHER_INDEX,
        COLOR_MIXER,
        MENU_BUTTON_VISIBLE,
        PANE_BUTTON_LOOKUP_CALLBACK,
        SEARCH_BOX_VISIBLE,
        SEARCH_LOUPE_VISIBLE,
        SEARCH_LISTENER,
        BACK_BUTTON_VISIBLE,
        BACK_BUTTON_ENABLED,
        BACK_BUTTON_LISTENER,
        IS_INCOGNITO,
        APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION,
        HUB_SEARCH_ENABLED_STATE,
        HAIRLINE_VISIBILITY,
    };
}
