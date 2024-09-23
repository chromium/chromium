// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

public class CommerceBottomSheetContentProperties {
    public static final ReadableIntPropertyKey TYPE = new ReadableIntPropertyKey();
    public static final ReadableBooleanPropertyKey HAS_TITLE = new ReadableBooleanPropertyKey();

    public static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<View> CUSTOM_VIEW =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TYPE, HAS_TITLE, TITLE, CUSTOM_VIEW};
}
