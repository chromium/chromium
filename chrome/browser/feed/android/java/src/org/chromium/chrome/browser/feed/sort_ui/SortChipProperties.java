// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.sort_ui;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties for a sorting option. */
public class SortChipProperties {
    public static final PropertyModel.ReadableObjectPropertyKey<String> NAME_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_SELECT_CALLBACK_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.ReadableBooleanPropertyKey IS_INITIALLY_SELECTED_KEY =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {NAME_KEY, ON_SELECT_CALLBACK_KEY, IS_INITIALLY_SELECTED_KEY};
}
