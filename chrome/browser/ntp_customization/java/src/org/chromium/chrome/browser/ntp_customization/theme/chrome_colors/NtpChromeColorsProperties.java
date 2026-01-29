// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.chrome_colors;

import android.text.TextWatcher;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the NTP customization Chrome Colors bottom sheet. */
@NullMarked
public class NtpChromeColorsProperties {
    public static final WritableObjectPropertyKey<View.OnClickListener> BACK_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener> SAVE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<TextWatcher> BACKGROUND_COLOR_INPUT_TEXT_WATCHER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<TextWatcher> PRIMARY_COLOR_INPUT_TEXT_WATCHER =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR_CIRCLE_VIEW_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey PRIMARY_COLOR_CIRCLE_VIEW_COLOR =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<RecyclerView.LayoutManager>
            RECYCLER_VIEW_LAYOUT_MANAGER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<RecyclerView.Adapter> RECYCLER_VIEW_ADAPTER =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey RECYCLER_VIEW_ITEM_WIDTH =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey RECYCLER_VIEW_SPACING = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey RECYCLER_VIEW_MAX_ITEM_COUNT =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<OnCheckedChangeListener>
            DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER = new WritableObjectPropertyKey<>();

    // This value isn't updated when the DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER handles the
    // clicking of the toggle. We have to skip equality check to allow the refreshed state is set.
    public static final WritableObjectPropertyKey<Boolean> IS_DAILY_REFRESH_SWITCH_CHECKED =
            new WritableObjectPropertyKey(/* skipEquality= */ true);

    // This index isn't updated when the RECYCLER_VIEW_ADAPTER handles the highlighted items. We
    // have to skip equality check to allow the refreshed value is set. The default 0 is set when
    // user turns on daily refresh without choosing any color.
    public static final WritableObjectPropertyKey<Integer> HIGHLIGHTED_ITEM_INDEX =
            new WritableObjectPropertyKey(/* skipEquality= */ true);

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BACK_BUTTON_CLICK_LISTENER,
                SAVE_BUTTON_CLICK_LISTENER,
                BACKGROUND_COLOR_INPUT_TEXT_WATCHER,
                PRIMARY_COLOR_INPUT_TEXT_WATCHER,
                BACKGROUND_COLOR_CIRCLE_VIEW_COLOR,
                PRIMARY_COLOR_CIRCLE_VIEW_COLOR,
                CUSTOM_COLOR_PICKER_CONTAINER_VISIBILITY,
                RECYCLER_VIEW_LAYOUT_MANAGER,
                RECYCLER_VIEW_ADAPTER,
                RECYCLER_VIEW_ITEM_WIDTH,
                RECYCLER_VIEW_SPACING,
                RECYCLER_VIEW_MAX_ITEM_COUNT,
                DAILY_REFRESH_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                IS_DAILY_REFRESH_SWITCH_CHECKED,
                HIGHLIGHTED_ITEM_INDEX
            };
}
