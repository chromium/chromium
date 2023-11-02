// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;

import android.text.TextWatcher;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the tasks surface properties. */
public class TasksSurfaceProperties {
    private TasksSurfaceProperties() {}

    public static final PropertyModel.WritableBooleanPropertyKey IS_FAKE_SEARCH_BOX_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableBooleanPropertyKey IS_INCOGNITO_DESCRIPTION_INITIALIZED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO_DESCRIPTION_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_LENS_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SURFACE_BODY_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_CAROUSEL_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_CAROUSEL_TITLE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableBooleanPropertyKey IS_VOICE_RECOGNITION_BUTTON_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableBooleanPropertyKey INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnCheckedChangeListener>
            INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableIntPropertyKey INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<IncognitoCookieControlsManager>
            INCOGNITO_COOKIE_CONTROLS_MANAGER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> INCOGNITO_LEARN_MORE_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> FAKE_SEARCH_BOX_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<TextWatcher> FAKE_SEARCH_BOX_TEXT_WATCHER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> LENS_BUTTON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> MORE_TABS_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey MV_TILES_VISIBLE =
            IS_CONTAINER_VISIBLE;
    public static final PropertyModel.WritableBooleanPropertyKey QUERY_TILES_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> VOICE_SEARCH_BUTTON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey TASKS_SURFACE_BODY_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey MV_TILES_CONTAINER_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey TAB_SWITCHER_TITLE_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey SINGLE_TAB_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey TOP_TOOLBAR_PLACEHOLDER_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey RESET_TASK_SURFACE_HEADER_SCROLL_POSITION =
            new PropertyModel.WritableObjectPropertyKey<>(true /* skipEquality */);
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_FAKE_SEARCH_BOX_VISIBLE,
            IS_INCOGNITO, IS_INCOGNITO_DESCRIPTION_INITIALIZED, IS_INCOGNITO_DESCRIPTION_VISIBLE,
            IS_LENS_BUTTON_VISIBLE, IS_SURFACE_BODY_VISIBLE, IS_TAB_CAROUSEL_VISIBLE,
            IS_TAB_CAROUSEL_TITLE_VISIBLE, IS_VOICE_RECOGNITION_BUTTON_VISIBLE,
            INCOGNITO_COOKIE_CONTROLS_ICON_CLICK_LISTENER, INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED,
            INCOGNITO_COOKIE_CONTROLS_TOGGLE_CHECKED_LISTENER,
            INCOGNITO_COOKIE_CONTROLS_TOGGLE_ENFORCEMENT, INCOGNITO_COOKIE_CONTROLS_MANAGER,
            INCOGNITO_LEARN_MORE_CLICK_LISTENER, FAKE_SEARCH_BOX_CLICK_LISTENER,
            FAKE_SEARCH_BOX_TEXT_WATCHER, LENS_BUTTON_CLICK_LISTENER, MORE_TABS_CLICK_LISTENER,
            MV_TILES_VISIBLE, QUERY_TILES_VISIBLE, VOICE_SEARCH_BUTTON_CLICK_LISTENER,
            TASKS_SURFACE_BODY_TOP_MARGIN, MV_TILES_CONTAINER_TOP_MARGIN,
            MV_TILES_CONTAINER_LEFT_RIGHT_MARGIN, TAB_SWITCHER_TITLE_TOP_MARGIN,
            SINGLE_TAB_TOP_MARGIN, RESET_TASK_SURFACE_HEADER_SCROLL_POSITION,
            TOP_TOOLBAR_PLACEHOLDER_HEIGHT};
}
