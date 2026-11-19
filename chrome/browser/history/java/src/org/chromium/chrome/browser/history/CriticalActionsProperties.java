// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** PropertyModel keys for the critical actions section of a history row. */
@NullMarked
class CriticalActionsProperties {
    @IntDef({ItemType.ACTION})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int ACTION = 0;
    }

    /** Whether the row's expand/collapse chevron is visible. */
    static final WritableBooleanPropertyKey EXPAND_ICON_VISIBLE = new WritableBooleanPropertyKey();

    /** Accessibility content description summarizing the action count on the chevron. */
    static final WritableObjectPropertyKey<@Nullable CharSequence> EXPAND_ICON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Whether the below-row "Gemini actions" panel is expanded, and whether to animate to it. */
    static final class Expansion {
        static final Expansion COLLAPSED =
                new Expansion(/* expanded= */ false, /* animate= */ false);
        static final Expansion EXPANDED = new Expansion(/* expanded= */ true, /* animate= */ false);
        static final Expansion ANIMATE_TO_COLLAPSED =
                new Expansion(/* expanded= */ false, /* animate= */ true);
        static final Expansion ANIMATE_TO_EXPANDED =
                new Expansion(/* expanded= */ true, /* animate= */ true);

        public final boolean expanded;
        public final boolean animate;

        private Expansion(boolean expanded, boolean animate) {
            this.expanded = expanded;
            this.animate = animate;
        }

        static Expansion of(boolean expanded, boolean animate) {
            if (animate) return expanded ? ANIMATE_TO_EXPANDED : ANIMATE_TO_COLLAPSED;
            return expanded ? EXPANDED : COLLAPSED;
        }
    }

    /** Whether the panel is expanded, and whether to animate to that state. */
    static final WritableObjectPropertyKey<Expansion> EXPANSION = new WritableObjectPropertyKey<>();

    /** Toggles the panel when the chevron is clicked. Null when there are no critical actions. */
    static final WritableObjectPropertyKey<@Nullable Runnable> EXPAND_ICON_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    /** Accessibility click label of the chevron ("Expand section" / "Collapse section"). */
    static final WritableObjectPropertyKey<@Nullable CharSequence>
            EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        EXPAND_ICON_VISIBLE,
        EXPAND_ICON_CONTENT_DESCRIPTION,
        EXPANSION,
        EXPAND_ICON_CLICK_HANDLER,
        EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL,
    };

    /** Localized label of an individual critical action row. */
    static final ReadableObjectPropertyKey<String> LABEL = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ACTION_ITEM_KEYS = {LABEL};
}
