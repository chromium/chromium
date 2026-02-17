// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Property keys for a generic two-cell educational tip module. Defines properties for an overall
 * module title and for each of the two vertically stacked cells.
 */
@NullMarked
public class EducationalTipModuleTwoCellProperties {
    /** The title of the module. */
    public static final WritableObjectPropertyKey<String> MODULE_TITLE =
            new WritableObjectPropertyKey<>();

    /** The OnClickListener for the "See more" text view */
    public static final WritableObjectPropertyKey<Runnable> SEE_MORE_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    // Two-cell layout properties
    public static final WritableObjectPropertyKey<String> ITEM_1_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> ITEM_1_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> ITEM_1_COMPLETED_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ITEM_1_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> ITEM_1_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> ITEM_1_MARK_COMPLETED =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> ITEM_2_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> ITEM_2_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Integer> ITEM_2_COMPLETED_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> ITEM_2_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> ITEM_2_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Boolean> ITEM_2_MARK_COMPLETED =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        MODULE_TITLE,
        SEE_MORE_CLICK_HANDLER,
        ITEM_1_TITLE,
        ITEM_1_DESCRIPTION,
        ITEM_1_ICON,
        ITEM_1_COMPLETED_ICON,
        ITEM_1_CLICK_HANDLER,
        ITEM_1_MARK_COMPLETED,
        ITEM_2_TITLE,
        ITEM_2_DESCRIPTION,
        ITEM_2_ICON,
        ITEM_2_COMPLETED_ICON,
        ITEM_2_CLICK_HANDLER,
        ITEM_2_MARK_COMPLETED
    };
}
