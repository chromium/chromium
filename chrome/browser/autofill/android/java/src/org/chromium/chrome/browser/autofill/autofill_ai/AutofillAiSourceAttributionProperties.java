// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Model properties for the Autofill AI source attribution bottom sheet. */
@NullMarked
/*package*/ class AutofillAiSourceAttributionProperties {
    @IntDef({ItemType.HEADER, ItemType.SOURCE_CARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ItemType {
        int HEADER = 0;
        int SOURCE_CARD = 1;
    }

    public static class HeaderProperties {
        public static final WritableObjectPropertyKey<String> SUBTITLE =
                new WritableObjectPropertyKey<>("subtitle");

        public static final PropertyKey[] ALL_KEYS = {SUBTITLE};

        private HeaderProperties() {}
    }

    public static class SourceCardProperties {
        public static final ReadableIntPropertyKey ICON_RES_ID =
                new ReadableIntPropertyKey("icon_res_id");
        public static final ReadableObjectPropertyKey<String> TITLE =
                new ReadableObjectPropertyKey<>("title");
        public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
                new ReadableObjectPropertyKey<>("content_description");
        public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
                new ReadableObjectPropertyKey<>("on_click_listener");

        public static final PropertyKey[] ALL_KEYS = {
            ICON_RES_ID, TITLE, CONTENT_DESCRIPTION, ON_CLICK_LISTENER
        };

        private SourceCardProperties() {}
    }

    private AutofillAiSourceAttributionProperties() {}
}
