// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabSideSheetStrategy.MaximizeButtonCallback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
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
        public SideSheetMaximizeButtonData() {
            this(false, false, () -> false);
        }
    }

    /** Property key for the side sheet maximize button. */
    public static final WritableObjectPropertyKey<SideSheetMaximizeButtonData>
            SIDE_SHEET_MAXIMIZE_BUTTON = new WritableObjectPropertyKey<>();

    public static PropertyModel create(
            PropertyListModel<PropertyModel, PropertyKey> customActionButtons) {
        return new PropertyModel.Builder(CUSTOM_ACTION_BUTTONS, SIDE_SHEET_MAXIMIZE_BUTTON)
                .with(CUSTOM_ACTION_BUTTONS, customActionButtons)
                .with(SIDE_SHEET_MAXIMIZE_BUTTON, new SideSheetMaximizeButtonData())
                .build();
    }
}
