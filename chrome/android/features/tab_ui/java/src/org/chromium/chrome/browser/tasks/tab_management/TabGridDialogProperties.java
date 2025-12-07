// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** List of properties used by TabGridDialog. */
@NullMarked
class TabGridDialogProperties {
    public static final ReadableObjectPropertyKey<BrowserControlsStateProvider>
            BROWSER_CONTROLS_STATE_PROVIDER = new ReadableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> COLLAPSE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> ADD_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener> SHARE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<OnClickListener>
            SHARE_IMAGE_TILES_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> HEADER_TITLE =
            new WritableObjectPropertyKey<>(true);
    public static final WritableIntPropertyKey CONTENT_TOP_MARGIN = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey APP_HEADER_HEIGHT = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PRIMARY_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DIALOG_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<ColorStateList> TINT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> IS_DIALOG_VISIBLE =
            new WritableObjectPropertyKey<>(true);
    public static final WritableBooleanPropertyKey SHOW_SHARE_BUTTON =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey SHARE_BUTTON_STRING_RES =
            new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey SHOW_IMAGE_TILES =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<TabGridDialogView.VisibilityListener>
            VISIBILITY_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> SCRIMVIEW_CLICK_RUNNABLE =
            new WritableObjectPropertyKey<>(true);
    public static final WritableObjectPropertyKey<View> ANIMATION_SOURCE_VIEW =
            new WritableObjectPropertyKey<>(true);
    public static final WritableIntPropertyKey UNGROUP_BAR_STATUS = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DIALOG_UNGROUP_BAR_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DIALOG_UNGROUP_BAR_TEXT_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> DIALOG_UNGROUP_BAR_TEXT =
            new WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link WritableIntPropertyKey} so that we can force update on the same
     * value.
     */
    public static final WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new WritableObjectPropertyKey<>(true);

    public static final WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> MENU_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<TextWatcher> TITLE_TEXT_WATCHER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnFocusChangeListener>
            TITLE_TEXT_ON_FOCUS_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey TITLE_CURSOR_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_TITLE_TEXT_FOCUSED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_KEYBOARD_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<String> COLLAPSE_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey TAB_GROUP_COLOR_ID = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey IS_INCOGNITO = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> COLOR_ICON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey HAIRLINE_COLOR = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey HAIRLINE_VISIBILITY =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey FORCE_ANIMATION_TO_FINISH =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_CONTENT_SENSITIVE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey SHOW_SEND_FEEDBACK =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<Runnable> SEND_FEEDBACK_RUNNABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<TabKeyEventData>> PAGE_KEY_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SUPPRESS_ACCESSIBILITY =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BROWSER_CONTROLS_STATE_PROVIDER,
                COLLAPSE_CLICK_LISTENER,
                ADD_CLICK_LISTENER,
                SHARE_BUTTON_CLICK_LISTENER,
                SHARE_IMAGE_TILES_CLICK_LISTENER,
                HEADER_TITLE,
                PRIMARY_COLOR,
                DIALOG_BACKGROUND_COLOR,
                TINT,
                VISIBILITY_LISTENER,
                SCRIMVIEW_CLICK_RUNNABLE,
                ANIMATION_SOURCE_VIEW,
                UNGROUP_BAR_STATUS,
                DIALOG_UNGROUP_BAR_BACKGROUND_COLOR,
                DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR,
                DIALOG_UNGROUP_BAR_TEXT_COLOR,
                DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR,
                DIALOG_UNGROUP_BAR_TEXT,
                MENU_CLICK_LISTENER,
                TITLE_TEXT_WATCHER,
                TITLE_TEXT_ON_FOCUS_LISTENER,
                TITLE_CURSOR_VISIBILITY,
                IS_TITLE_TEXT_FOCUSED,
                IS_KEYBOARD_VISIBLE,
                COLLAPSE_BUTTON_CONTENT_DESCRIPTION,
                IS_DIALOG_VISIBLE,
                SHOW_SHARE_BUTTON,
                SHARE_BUTTON_STRING_RES,
                SHOW_IMAGE_TILES,
                CONTENT_TOP_MARGIN,
                APP_HEADER_HEIGHT,
                IS_MAIN_CONTENT_VISIBLE,
                INITIAL_SCROLL_INDEX,
                TAB_GROUP_COLOR_ID,
                IS_INCOGNITO,
                COLOR_ICON_CLICK_LISTENER,
                HAIRLINE_COLOR,
                HAIRLINE_VISIBILITY,
                FORCE_ANIMATION_TO_FINISH,
                IS_CONTENT_SENSITIVE,
                SHOW_SEND_FEEDBACK,
                SEND_FEEDBACK_RUNNABLE,
                PAGE_KEY_LISTENER,
                SUPPRESS_ACCESSIBILITY,
            };
}
