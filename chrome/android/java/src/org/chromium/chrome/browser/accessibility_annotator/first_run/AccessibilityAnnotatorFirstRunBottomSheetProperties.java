// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Accessibility Annotator first-run bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorFirstRunBottomSheetProperties {
    public static final WritableObjectPropertyKey<CharSequence> CARD_1_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> CARD_2_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    static final ReadableIntPropertyKey ICON = new ReadableIntPropertyKey();
    public static final WritableObjectPropertyKey<CharSequence> LEARN_MORE_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> PRIMARY_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> SECONDARY_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        CARD_1_TEXT,
        CARD_2_TEXT,
        DESCRIPTION,
        ICON,
        LEARN_MORE_DESCRIPTION,
        PRIMARY_BUTTON_LABEL,
        SECONDARY_BUTTON_LABEL,
        TITLE
    };

    private AccessibilityAnnotatorFirstRunBottomSheetProperties() {}
}
