// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Model for the Status view.
 */
class StatusProperties {
    /** Enables / disables animations. */
    static final WritableBooleanPropertyKey ANIMATIONS_ENABLED = new WritableBooleanPropertyKey();

    /** Specifies navigation icon resource type .*/
    static final WritableIntPropertyKey STATUS_ICON_RES = new WritableIntPropertyKey();

    /** Specifies color tint for navigation icon. */
    static final WritableIntPropertyKey STATUS_ICON_TINT_RES = new WritableIntPropertyKey();

    /** Specifies the icon. */
    static final WritableObjectPropertyKey<Bitmap> STATUS_ICON =
            new WritableObjectPropertyKey<>(true);

    /** Specifies the icon alpha. */
    static final WritableFloatPropertyKey STATUS_ALPHA = new WritableFloatPropertyKey();

    /** Specifies if the icon should be shown or not. */
    static final WritableBooleanPropertyKey SHOW_STATUS_ICON = new WritableBooleanPropertyKey();

    /** Specifies accessibility string presented to user upon long click on security icon. */
    public static final WritableIntPropertyKey STATUS_ICON_ACCESSIBILITY_TOAST_RES =
            new WritableIntPropertyKey();

    /** Specifies string resource holding content description for security icon. */
    static final WritableIntPropertyKey STATUS_ICON_DESCRIPTION_RES = new WritableIntPropertyKey();

    /** Specifies status separator color. */
    static final WritableIntPropertyKey SEPARATOR_COLOR_RES = new WritableIntPropertyKey();

    /** Specifies object to receive status click events. */
    static final WritableObjectPropertyKey<View.OnClickListener> STATUS_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_COLOR_RES =
            new WritableIntPropertyKey();

    /** Specifies content of the verbose status text field. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_STRING_RES =
            new WritableIntPropertyKey();

    /** Specifies whether verbose status text view is visible. */
    static final WritableBooleanPropertyKey VERBOSE_STATUS_TEXT_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Specifies width of the verbose status text view. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_WIDTH = new WritableIntPropertyKey();

    /** Specifies whether the incognito badge is visible or not. */
    static final WritableBooleanPropertyKey INCOGNITO_BADGE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ANIMATIONS_ENABLED,
            STATUS_ICON_ACCESSIBILITY_TOAST_RES, STATUS_ICON_RES, STATUS_ICON_TINT_RES, STATUS_ICON,
            STATUS_ALPHA, SHOW_STATUS_ICON, STATUS_ICON_DESCRIPTION_RES, SEPARATOR_COLOR_RES,
            STATUS_CLICK_LISTENER, VERBOSE_STATUS_TEXT_COLOR_RES, VERBOSE_STATUS_TEXT_STRING_RES,
            VERBOSE_STATUS_TEXT_VISIBLE, VERBOSE_STATUS_TEXT_WIDTH, INCOGNITO_BADGE_VISIBLE};

    private StatusProperties() {}
}
