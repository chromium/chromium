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

    /** The mvt section subtitle in the main bottom sheet. */
    public static final PropertyModel.WritableIntPropertyKey
            MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE = new PropertyModel.WritableIntPropertyKey();

    /** The New Tab Page Cards section subtitle in the main bottom sheet. */
    public static final PropertyModel.WritableIntPropertyKey
            MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID =
                    new PropertyModel.WritableIntPropertyKey();

    /** The keys to bind a {@link BottomSheetListContainerView}. */
    public static final PropertyKey[] LIST_CONTAINER_KEYS =
            new PropertyKey[] {
                LIST_CONTAINER_VIEW_DELEGATE,
                MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE,
                MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE,
                MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID,
            };

    /** The click listener to handle back button clicks in the bottom sheet. */
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            BACK_PRESS_HANDLER = new PropertyModel.WritableObjectPropertyKey<>();

    /** The checked state of the MVT visibility switch within the MVT bottom sheet. */
    public static final PropertyModel.WritableBooleanPropertyKey IS_MVT_SWITCH_CHECKED =
            new PropertyModel.WritableBooleanPropertyKey();

    /** The on checked change listener for the MVT visibility switch within the MVT bottom sheet. */
    public static final PropertyModel.WritableObjectPropertyKey<OnCheckedChangeListener>
            MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    /** The resource ID of the content description for the switch within the MVT bottom sheet. */
    public static final PropertyModel.WritableObjectPropertyKey<Integer>
            SET_MVT_SWITCH_CONTENT_DESCRIPTION_RES_ID =
                    new PropertyModel.WritableObjectPropertyKey<>();

    /** The keys to bind a NTP customization bottom sheet with a back button inside. */
    public static final PropertyKey[] BOTTOM_SHEET_KEYS =
            new PropertyKey[] {
                BACK_PRESS_HANDLER,
                IS_MVT_SWITCH_CHECKED,
                MVT_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                SET_MVT_SWITCH_CONTENT_DESCRIPTION_RES_ID,
            };

    // Properties specifically for the NTP cards bottom sheet.

    /** {@link OnCheckedChangeListener} called when the "all NTP cards" switch is toggled. */
    public static final PropertyModel.WritableObjectPropertyKey<OnCheckedChangeListener>
            ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER =
                    new PropertyModel.WritableObjectPropertyKey<>();

    /** Whether individual cards' switches should be enabled (true iff "all NTP cards" is on). */
    public static final PropertyModel.WritableBooleanPropertyKey ARE_CARD_SWITCHES_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] NTP_CARD_SETTINGS_KEYS =
            new PropertyKey[] {
                ALL_NTP_CARDS_SWITCH_ON_CHECKED_CHANGE_LISTENER, ARE_CARD_SWITCHES_ENABLED
            };

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

    public static final PropertyModel.WritableObjectPropertyKey<String>
            SET_FEED_SWITCH_CONTENT_DESCRIPTION = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] FEED_SETTINGS_KEYS =
            new PropertyKey[] {
                FEED_SWITCH_ON_CHECKED_CHANGE_LISTENER,
                IS_FEED_SWITCH_CHECKED,
                IS_FEED_LIST_ITEMS_TITLE_VISIBLE,
                LEARN_MORE_BUTTON_CLICK_LISTENER,
                SET_FEED_SWITCH_CONTENT_DESCRIPTION
            };
}
