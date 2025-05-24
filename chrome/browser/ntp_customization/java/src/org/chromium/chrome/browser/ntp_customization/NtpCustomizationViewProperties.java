// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The properties associated with rendering NTP customization bottom sheets. */
@NullMarked
public class NtpCustomizationViewProperties {
    // Properties for the general bottom sheet:

    /** The position of the bottom sheet layout in the view flipper. */
    public static final PropertyModel.WritableIntPropertyKey LAYOUT_TO_DISPLAY =
            new PropertyModel.WritableIntPropertyKey();

    /** The keys to bind a view flipper view. */
    public static final PropertyKey[] VIEW_FLIPPER_KEYS = new PropertyKey[] {LAYOUT_TO_DISPLAY};

    /**
     * The delegate provides list content and event handlers to a {@link
     * BottomSheetListContainerView}.
     */
    public static final PropertyModel.WritableObjectPropertyKey<ListContainerViewDelegate>
            LIST_CONTAINER_VIEW_DELEGATE = new PropertyModel.WritableObjectPropertyKey<>();

    /** The feed section subtitle in the main bottom sheet. */
    public static final PropertyModel.WritableIntPropertyKey
            MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE = new PropertyModel.WritableIntPropertyKey();

    /** The keys to bind a {@link BottomSheetListContainerView}. */
    public static final PropertyKey[] LIST_CONTAINER_KEYS =
            new PropertyKey[] {
                LIST_CONTAINER_VIEW_DELEGATE, MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE
            };

    /** The click listener to handle back button clicks in the bottom sheet. */
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            BACK_PRESS_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();

    /** The keys to bind a NTP customization bottom sheet with a back button inside. */
    public static final PropertyKey[] BOTTOM_SHEET_KEYS = new PropertyKey[] {BACK_PRESS_HANDLER};

    // Properties specifically for the feed settings bottom sheet:

    public static final PropertyModel.WritableObjectPropertyKey<OnCheckedChangeListener>
            FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER =
                    new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            LEARN_MORE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey IS_FEED_SWITCH_CHECKED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_FEED_LIST_ITEMS_TITLE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] FEED_SETTINGS_KEYS =
            new PropertyKey[] {
                FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                IS_FEED_SWITCH_CHECKED,
                IS_FEED_LIST_ITEMS_TITLE_VISIBLE,
                LEARN_MORE_BUTTON_CLICK_LISTENER
            };
}
