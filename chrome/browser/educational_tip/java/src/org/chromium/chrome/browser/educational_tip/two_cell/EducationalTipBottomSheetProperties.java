// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The properties associated with rendering the educational tip bottom sheet. */
@NullMarked
public class EducationalTipBottomSheetProperties {
    public static final WritableObjectPropertyKey<String> BOTTOM_SHEET_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> BOTTOM_SHEET_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<List<EducationalTipBottomSheetItem>>
            BOTTOM_SHEET_LIST_ITEMS = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Runnable> BOTTOM_SHEET_LIST_ITEMS_ON_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_TITLE,
        BOTTOM_SHEET_DESCRIPTION,
        BOTTOM_SHEET_LIST_ITEMS,
        BOTTOM_SHEET_LIST_ITEMS_ON_CLICK,
    };
}
