// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
/* Properties for the composeplate on the NTP. */
interface ComposeplateProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<View.OnClickListener> COMPOSEPLATE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey APPLY_WHITE_BACKGROUND = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<@Nullable ColorStateList> COLOR_STATE_LIST =
            new WritableObjectPropertyKey<>();
    WritableIntPropertyKey TEXT_STYLE_RES_ID = new WritableIntPropertyKey();

    /** The UI config of the AI Mode button offered by the default search engine. */
    WritableObjectPropertyKey<AiModeButtonUiConfig> AI_MODE_BUTTON_UI_CONFIG =
            new WritableObjectPropertyKey<>();

    /**
     * The icon of the AI Mode button, together with whether it should be tinted. Kept in a single
     * object so that the drawable and its tinting are always bound atomically.
     */
    final class AiModeButtonIcon {
        public final @Nullable Drawable drawable;

        /**
         * Whether the icon should be tinted. False for full color icons, e.g. a favicon fetched
         * from the search engine, which would otherwise be rendered as a solid silhouette.
         */
        public final boolean shouldTint;

        public AiModeButtonIcon(@Nullable Drawable drawable, boolean shouldTint) {
            this.drawable = drawable;
            this.shouldTint = shouldTint;
        }
    }

    /** The icon of the AI Mode button. */
    WritableObjectPropertyKey<AiModeButtonIcon> AI_MODE_BUTTON_ICON =
            new WritableObjectPropertyKey<>();

    /** Whether the optional button is visible. */
    WritableBooleanPropertyKey IS_OPTIONAL_BUTTON_VISIBLE = new WritableBooleanPropertyKey();

    /** The resource id of the optional button's icon drawable. */
    WritableIntPropertyKey OPTIONAL_BUTTON_ICON_RES_ID = new WritableIntPropertyKey();

    /** The click listener of the optional button. */
    WritableObjectPropertyKey<View.@Nullable OnClickListener> OPTIONAL_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The text of the optional button. It is only shown with the large form factor layout. */
    WritableObjectPropertyKey<@Nullable String> OPTIONAL_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /**
     * The content description of the optional button, also used as its tooltip. It is more
     * descriptive than {@link #OPTIONAL_BUTTON_TEXT}, since the button is icon-only on mobiles.
     */
    WritableObjectPropertyKey<@Nullable String> OPTIONAL_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /**
     * The click listener of the trailing button, i.e., the last button of the composeplate. The
     * trailing button shows the incognito icon and strings declared in the layout, unless the icon,
     * text and content description below are set.
     */
    WritableObjectPropertyKey<View.@Nullable OnClickListener> TRAILING_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The resource id of the trailing button's icon drawable. */
    WritableIntPropertyKey TRAILING_BUTTON_ICON_RES_ID = new WritableIntPropertyKey();

    /** The text of the trailing button. */
    WritableObjectPropertyKey<@Nullable String> TRAILING_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** The content description of the trailing button, also used as its tooltip. */
    WritableObjectPropertyKey<@Nullable String> TRAILING_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The lateral (start and end) padding of the optional and trailing buttons, in pixels. */
    WritableIntPropertyKey OPTIONAL_BUTTON_LATERAL_PADDING = new WritableIntPropertyKey();

    /** The end margin of the composeplate and optional buttons, in pixels. */
    WritableIntPropertyKey OPTIONAL_BUTTON_MARGIN_END = new WritableIntPropertyKey();

    /** Whether the text of the trailing button is visible. */
    WritableBooleanPropertyKey IS_TRAILING_BUTTON_TEXT_VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the text of the optional button is visible. */
    WritableBooleanPropertyKey IS_OPTIONAL_BUTTON_TEXT_VISIBLE = new WritableBooleanPropertyKey();

    /**
     * Whether the optional button wraps its content. Otherwise, it shares the width equally with
     * the other weighted buttons.
     */
    WritableBooleanPropertyKey IS_OPTIONAL_BUTTON_WRAP_CONTENT = new WritableBooleanPropertyKey();

    /**
     * Whether the trailing button wraps its content. Otherwise, it shares the width equally with
     * the other weighted buttons.
     */
    WritableBooleanPropertyKey IS_TRAILING_BUTTON_WRAP_CONTENT = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                COMPOSEPLATE_BUTTON_CLICK_LISTENER,
                APPLY_WHITE_BACKGROUND,
                COLOR_STATE_LIST,
                TEXT_STYLE_RES_ID,
                AI_MODE_BUTTON_UI_CONFIG,
                AI_MODE_BUTTON_ICON,
                IS_OPTIONAL_BUTTON_VISIBLE,
                OPTIONAL_BUTTON_ICON_RES_ID,
                OPTIONAL_BUTTON_CLICK_LISTENER,
                OPTIONAL_BUTTON_TEXT,
                OPTIONAL_BUTTON_CONTENT_DESCRIPTION,
                TRAILING_BUTTON_CLICK_LISTENER,
                TRAILING_BUTTON_ICON_RES_ID,
                TRAILING_BUTTON_TEXT,
                TRAILING_BUTTON_CONTENT_DESCRIPTION,
                OPTIONAL_BUTTON_LATERAL_PADDING,
                OPTIONAL_BUTTON_MARGIN_END,
                IS_TRAILING_BUTTON_TEXT_VISIBLE,
                IS_TRAILING_BUTTON_WRAP_CONTENT,
                IS_OPTIONAL_BUTTON_TEXT_VISIBLE,
                IS_OPTIONAL_BUTTON_WRAP_CONTENT,
            };
}
