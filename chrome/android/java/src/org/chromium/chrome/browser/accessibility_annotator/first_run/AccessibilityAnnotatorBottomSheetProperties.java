// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Accessibility Annotator bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorBottomSheetProperties {
    public static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();
    static final ReadableIntPropertyKey ICON = new ReadableIntPropertyKey();
    static final ReadableObjectPropertyKey<String> PRIMARY_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> SECONDARY_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        DESCRIPTION, ICON, PRIMARY_BUTTON_LABEL, SECONDARY_BUTTON_LABEL, TITLE
    };

    private AccessibilityAnnotatorBottomSheetProperties() {}
}
