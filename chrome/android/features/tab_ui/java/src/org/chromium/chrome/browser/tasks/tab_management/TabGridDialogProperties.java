// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** List of properties used by TabGridDialog. */
class TabGridDialogProperties {
    /** The hashCode() of the {@link TabGridDialogMediator} currently updating the view. */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> BINDING_TOKEN =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<BrowserControlsStateProvider>
            BROWSER_CONTROLS_STATE_PROVIDER = new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            COLLAPSE_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            ADD_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            SHARE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            SHARE_IMAGE_TILES_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<String> HEADER_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableIntPropertyKey CONTENT_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey APP_HEADER_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey PRIMARY_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<ColorStateList> TINT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_DIALOG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_SHARE_SHEET_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey SHOW_SHARE_BUTTON =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey SHOW_IMAGE_TILES =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<TabGridDialogView.VisibilityListener>
            VISIBILITY_LISTENER = new WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable> SCRIMVIEW_CLICK_RUNNABLE =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableObjectPropertyKey<View> ANIMATION_SOURCE_VIEW =
            new PropertyModel.WritableObjectPropertyKey<>(true);
    public static final PropertyModel.WritableIntPropertyKey UNGROUP_BAR_STATUS =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_UNGROUP_BAR_BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey
            DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR =
                    new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_UNGROUP_BAR_TEXT_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey DIALOG_UNGROUP_BAR_HOVERED_TEXT_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String> DIALOG_UNGROUP_BAR_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Integer>
            ANIMATION_BACKGROUND_COLOR = new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    public static final PropertyModel.WritableBooleanPropertyKey IS_MAIN_CONTENT_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            MENU_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<TextWatcher> TITLE_TEXT_WATCHER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnFocusChangeListener>
            TITLE_TEXT_ON_FOCUS_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey TITLE_CURSOR_VISIBILITY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_TITLE_TEXT_FOCUSED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_KEYBOARD_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<String>
            COLLAPSE_BUTTON_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey TAB_GROUP_COLOR_ID =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnClickListener>
            COLOR_ICON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey HAIRLINE_COLOR =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey HAIRLINE_VISIBILITY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey FORCE_ANIMATION_TO_FINISH =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BINDING_TOKEN,
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
                IS_SHARE_SHEET_VISIBLE,
                SHOW_SHARE_BUTTON,
                SHOW_IMAGE_TILES,
                CONTENT_TOP_MARGIN,
                APP_HEADER_HEIGHT,
                IS_MAIN_CONTENT_VISIBLE,
                INITIAL_SCROLL_INDEX,
                TAB_GROUP_COLOR_ID,
                IS_INCOGNITO,
                COLOR_ICON_CLICK_LISTENER,
                ANIMATION_BACKGROUND_COLOR,
                HAIRLINE_COLOR,
                HAIRLINE_VISIBILITY,
                FORCE_ANIMATION_TO_FINISH,
            };
}
