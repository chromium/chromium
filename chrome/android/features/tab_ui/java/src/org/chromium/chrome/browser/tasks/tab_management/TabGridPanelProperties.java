// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * List of properties used by TabGridDialog.
 */
class TabGridPanelProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> COLLAPSE_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> ADD_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnClickListener>();
    public static final PropertyModel.WritableObjectPropertyKey<String> HEADER_TITLE =
            new PropertyModel.WritableObjectPropertyKey<String>(true);
    public static final PropertyModel.WritableIntPropertyKey CONTENT_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> TINT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_DIALOG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<ScrimView.ScrimObserver> SCRIMVIEW_OBSERVER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View> ANIMATION_SOURCE_VIEW =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableIntPropertyKey UNGROUP_BAR_STATUS =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_BACKGROUND_RESOUCE_ID =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel
            .WritableIntPropertyKey DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel
            .WritableIntPropertyKey DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_UNGROUP_BAR_TEXT_APPEARANCE =
            new PropertyModel.WritableIntPropertyKey();
    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey(true);
    public static final PropertyModel.WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<OnClickListener> MENU_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<TextWatcher> TITLE_TEXT_WATCHER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnFocusChangeListener> TITLE_TEXT_ON_FOCUS_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey TITLE_CURSOR_VISIBILITY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TITLE_TEXT_FOCUSED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_POPUP_WINDOW_FOCUSABLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnTouchListener> TITLE_TEXT_ON_TOUCH_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<View.OnTouchListener>();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {COLLAPSE_CLICK_LISTENER,
            ADD_CLICK_LISTENER, HEADER_TITLE, CONTENT_TOP_MARGIN, PRIMARY_COLOR, TINT,
            IS_DIALOG_VISIBLE, SCRIMVIEW_OBSERVER, ANIMATION_SOURCE_VIEW, UNGROUP_BAR_STATUS,
            DIALOG_BACKGROUND_RESOUCE_ID, DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID,
            DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID, DIALOG_UNGROUP_BAR_TEXT_APPEARANCE,
            INITIAL_SCROLL_INDEX, IS_MAIN_CONTENT_VISIBLE, MENU_CLICK_LISTENER, TITLE_TEXT_WATCHER,
            TITLE_TEXT_ON_FOCUS_LISTENER, TITLE_CURSOR_VISIBILITY, IS_TITLE_TEXT_FOCUSED,
            IS_POPUP_WINDOW_FOCUSABLE, TITLE_TEXT_ON_TOUCH_LISTENER};
}
