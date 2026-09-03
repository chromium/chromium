// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties defined here reflect the visible state of the common TouchToFill components. */
@NullMarked
public final class TouchToFillCommonProperties {
    /** Properties defined here reflect the visible state of the header in the TouchToFill sheet. */
    public static final class HeaderProperties {
        public static final ReadableIntPropertyKey IMAGE_DRAWABLE_ID =
                new ReadableIntPropertyKey("image_drawable_id");
        public static final ReadableIntPropertyKey TITLE_ID =
                new ReadableIntPropertyKey("title_id");
        public static final ReadableIntPropertyKey SUBTITLE_ID =
                new ReadableIntPropertyKey("subtitle_id");
        public static final ReadableObjectPropertyKey<String> TITLE_STRING =
                new ReadableObjectPropertyKey<>("title_string");
        public static final ReadableIntPropertyKey TITLE_BOTTOM_MARGIN =
                new ReadableIntPropertyKey("header_title_bottom_margin");
        public static final ReadableIntPropertyKey SUBTITLE_BOTTOM_MARGIN =
                new ReadableIntPropertyKey("header_subtitle_bottom_margin");

        public static final PropertyKey[] ALL_KEYS = {
            IMAGE_DRAWABLE_ID,
            TITLE_ID,
            SUBTITLE_ID,
            TITLE_STRING,
            TITLE_BOTTOM_MARGIN,
            SUBTITLE_BOTTOM_MARGIN
        };

        private HeaderProperties() {}
    }

    /** Properties defined here reflect the visible state of a button in the TouchToFill sheet. */
    public static final class ButtonProperties {
        public static final ReadableIntPropertyKey TEXT_ID = new ReadableIntPropertyKey("text_id");
        public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_ACTION =
                new ReadableObjectPropertyKey<>("on_click_action");

        public static final PropertyKey[] ALL_KEYS = {TEXT_ID, ON_CLICK_ACTION};

        private ButtonProperties() {}
    }

    private TouchToFillCommonProperties() {}
}
