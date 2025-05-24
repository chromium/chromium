// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_DEFAULT;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.Px;
import androidx.browser.customtabs.CustomTabsIntent.CloseButtonPosition;

import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

public class CustomTabToolbarButtonsProperties {
    /** Whether the individual button is visible. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** Icon of the individual button. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    /** OnClickListener for the individual button. */
    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Description for the individual button. */
    public static final WritableObjectPropertyKey<String> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] INDIVIDUAL_BUTTON_KEYS = {
        VISIBLE, ICON, CLICK_LISTENER, DESCRIPTION
    };

    public static final WritableBooleanPropertyKey CUSTOM_ACTION_BUTTONS_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final ReadableObjectPropertyKey<PropertyListModel<PropertyModel, PropertyKey>>
            CUSTOM_ACTION_BUTTONS = new ReadableObjectPropertyKey<>();

    public static class SideSheetMaximizeButtonData {
        /** Whether the side-sheet maximize button is visible. */
        public final boolean visible;

        /** Whether the side-sheet is maximized. */
        public final boolean maximized;

        /** The {@link MaximizeButtonCallback} to notify of the click events. */
        public final MaximizeButtonCallback callback;

        SideSheetMaximizeButtonData(
                boolean visible, boolean maximized, MaximizeButtonCallback callback) {
            this.visible = visible;
            this.maximized = maximized;
            this.callback = callback;
        }

        /** Default constructor to hide the button. */
        SideSheetMaximizeButtonData() {
            this(false, false, () -> false);
        }
    }

    /** Property key for the side sheet maximize button. */
    public static final WritableObjectPropertyKey<SideSheetMaximizeButtonData>
            SIDE_SHEET_MAXIMIZE_BUTTON = new WritableObjectPropertyKey<>();

    public static class MinimizeButtonData {
        /** Whether the minimize button is visible. */
        public final boolean visible;

        /** The {@link OnClickListener} to notify of the click events. */
        public final OnClickListener clickListener;

        // TODO: Maybe add default constr for not visible
        MinimizeButtonData(boolean visible, OnClickListener clickListener) {
            this.visible = visible;
            this.clickListener = clickListener;
        }

        /** Default constructor to hide the button. */
        MinimizeButtonData() {
            this(false, v -> {});
        }
    }

    /** Property key for the minimize button. */
    public static final WritableObjectPropertyKey<MinimizeButtonData> MINIMIZE_BUTTON =
            new WritableObjectPropertyKey<>();

    public static class CloseButtonData {
        /** Whether the close button is visible. */
        public final boolean visible;

        /** The close button icon. */
        public final Drawable icon;

        /** The close button position. See {@link CloseButtonPosition}. */
        public final @CloseButtonPosition int position;

        // TODO: Maybe add default constr for not visible
        CloseButtonData(boolean visible, Drawable icon, @CloseButtonPosition int position) {
            this.visible = visible;
            this.icon = icon;
            this.position = position;
        }

        CloseButtonData() {
            this(false, null, CLOSE_BUTTON_POSITION_DEFAULT);
        }
    }

    /** Property key for the close button. */
    public static final ReadableObjectPropertyKey<CloseButtonData> CLOSE_BUTTON =
            new ReadableObjectPropertyKey<>();

    /** Property key for whether the menu button is visible. */
    public static final WritableBooleanPropertyKey MENU_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Property key for the toolbar width. */
    public static final WritableIntPropertyKey TOOLBAR_WIDTH = new WritableIntPropertyKey();

    /** Property key for whether the omnibox is enabled. */
    public static final ReadableBooleanPropertyKey OMNIBOX_ENABLED =
            new ReadableBooleanPropertyKey();

    /** Property key for whether the title is visible. */
    public static final ReadableBooleanPropertyKey TITLE_VISIBLE = new ReadableBooleanPropertyKey();

    /** Property key for whether the CCT is incognito. */
    public static final ReadableBooleanPropertyKey IS_INCOGNITO = new ReadableBooleanPropertyKey();

    public static PropertyModel create(
            boolean customActionButtonsVisible,
            PropertyListModel<PropertyModel, PropertyKey> customActionButtons,
            MinimizeButtonData minimizeButtonData,
            CloseButtonData closeButton,
            boolean menuButtonVisible,
            @Px int toolbarWidth,
            boolean omniboxEnabled,
            boolean titleVisible,
            boolean isIncognito) {
        return new PropertyModel.Builder(
                        CUSTOM_ACTION_BUTTONS_VISIBLE,
                        CUSTOM_ACTION_BUTTONS,
                        SIDE_SHEET_MAXIMIZE_BUTTON,
                        MINIMIZE_BUTTON,
                        CLOSE_BUTTON,
                        MENU_BUTTON_VISIBLE,
                        TOOLBAR_WIDTH,
                        OMNIBOX_ENABLED,
                        TITLE_VISIBLE,
                        IS_INCOGNITO)
                .with(CUSTOM_ACTION_BUTTONS_VISIBLE, customActionButtonsVisible)
                .with(CUSTOM_ACTION_BUTTONS, customActionButtons)
                .with(SIDE_SHEET_MAXIMIZE_BUTTON, new SideSheetMaximizeButtonData())
                .with(MINIMIZE_BUTTON, minimizeButtonData)
                .with(CLOSE_BUTTON, closeButton)
                .with(MENU_BUTTON_VISIBLE, menuButtonVisible)
                .with(TOOLBAR_WIDTH, toolbarWidth)
                .with(OMNIBOX_ENABLED, omniboxEnabled)
                .with(TITLE_VISIBLE, titleVisible)
                .with(IS_INCOGNITO, isIncognito)
                .build();
    }
}
