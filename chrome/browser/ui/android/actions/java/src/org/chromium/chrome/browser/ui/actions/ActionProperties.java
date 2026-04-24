// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for action buttons. */
@NullMarked
public class ActionProperties {
    public static final WritableIntPropertyKey ICON_ID = new WritableIntPropertyKey();

    /** Setting this property will override the drawable set by the ICON_ID property, if set. */
    public static final WritableObjectPropertyKey<Drawable> ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();

    /** This property should have a {@link ButtonState} value. */
    public static final WritableIntPropertyKey BUTTON_STATE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<TextResolver> CONTENT_DESCRIPTION_RESOLVER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<TextResolver> TOOLTIP_TEXT_RESOLVER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<View>> ON_PRESS_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<View>> ON_LONG_PRESS_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<IphIntent> IPH_INTENT =
            new WritableObjectPropertyKey<>();
    // TODO(b/502195080): Maybe remove this property if agreed on surface triggering it.
    public static final WritableObjectPropertyKey<UserEducationHelper> USER_EDUCATION_HELPER =
            new WritableObjectPropertyKey<>();

    /** Base properties for general action buttons. */
    public static final PropertyKey[] BASE_KEYS =
            new PropertyKey[] {
                ICON_ID,
                ICON_DRAWABLE,
                CONTENT_DESCRIPTION_RESOLVER,
                TOOLTIP_TEXT_RESOLVER,
                ON_PRESS_CALLBACK,
                ON_LONG_PRESS_CALLBACK,
                IPH_INTENT,
                USER_EDUCATION_HELPER,
            };

    /**
     * Base properties for action buttons that don't require providing an icon. This also includes
     * button state which is required for buttons with complex states like Tab Switcher.
     */
    public static final PropertyKey[] BASE_KEYS_WITH_BUTTON_STATE_AND_NO_ICON =
            new PropertyKey[] {
                BUTTON_STATE,
                CONTENT_DESCRIPTION_RESOLVER,
                TOOLTIP_TEXT_RESOLVER,
                ON_PRESS_CALLBACK,
                ON_LONG_PRESS_CALLBACK,
                IPH_INTENT,
                USER_EDUCATION_HELPER,
            };

    /** All properties for action buttons. */
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    new PropertyKey[] {ICON_ID, ICON_DRAWABLE},
                    BASE_KEYS_WITH_BUTTON_STATE_AND_NO_ICON);
}
