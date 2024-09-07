// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import android.text.SpannableString;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties defined here reflect the visible state of the simple notice sheet */
public class SimpleNoticeSheetProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> SHEET_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>("sheet_title");
    public static final PropertyModel.WritableObjectPropertyKey<SpannableString> SHEET_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("sheet_text");
    public static final PropertyModel.WritableObjectPropertyKey<String> BUTTON_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>("button_title");
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> BUTTON_ACTION =
            new PropertyModel.ReadableObjectPropertyKey<>("button_action");

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {SHEET_TITLE, SHEET_TEXT, BUTTON_TITLE, BUTTON_ACTION};
}
