// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering the educational tip bottom sheet. */
@NullMarked
public class EducationalTipBottomSheetProperties {
    // TODO(crbug.com/479597724): Use these properties.
    public static final WritableObjectPropertyKey<Integer> BOTTOM_SHEET_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> BOTTOM_SHEET_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> BOTTOM_SHEET_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    // TODO(crbug.com/479597724): Add other bottom sheet properties.

    public static final PropertyKey[] ALL_KEYS = {
        BOTTOM_SHEET_ICON, BOTTOM_SHEET_TITLE, BOTTOM_SHEET_DESCRIPTION
    };
}
