// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the start surface toolbar properties. */
class StartSurfaceToolbarProperties {
    private StartSurfaceToolbarProperties() {}

    public static final PropertyModel
            .WritableObjectPropertyKey<IncognitoStateProvider> INCOGNITO_STATE_PROVIDER =
            new PropertyModel.WritableObjectPropertyKey<IncognitoStateProvider>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> NEW_TAB_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> IDENTITY_DISC_CLICK_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<View.OnClickListener>();
    public static final PropertyModel.WritableObjectPropertyKey<Drawable> IDENTITY_DISC_IMAGE =
            new PropertyModel.WritableObjectPropertyKey<Drawable>(false);
    public static final PropertyModel.WritableIntPropertyKey IDENTITY_DISC_DESCRIPTION =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IDENTITY_DISC_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IN_START_SURFACE_MODE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey LOGO_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey ACCESSIBILITY_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey MENU_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_BUTTON_IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey BUTTONS_CLICKABLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_BUTTON_HIGHLIGHT =
            new PropertyModel.WritableBooleanPropertyKey();

    /** When set to true, move identity disc to the start of the toolbar. Can only set to true. */
    public static final PropertyModel.WritableBooleanPropertyKey IDENTITY_DISC_AT_START =
            new PropertyModel.WritableBooleanPropertyKey();

    /**
     * This is a hacky workaround for {@link IncognitoSwitchProperties#IS_VISIBLE}.
     * TODO(crbug.com/1042997): control the visibility through IncognitoSwitchCoordinator.
     */
    public static final PropertyModel.WritableObjectPropertyKey INCOGNITO_SWITCHER_VISIBLE =
            new PropertyModel.WritableObjectPropertyKey(true);
    /**
     * When set to true, move New Tab Button to the start of the toolbar, and move the Incognito
     * switcher to the center. Can only set to true.
     */
    public static final PropertyModel.WritableBooleanPropertyKey NEW_TAB_BUTTON_AT_START =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableFloatPropertyKey TRANSLATION_Y =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {NEW_TAB_CLICK_HANDLER, IS_VISIBLE, LOGO_IS_VISIBLE, IS_INCOGNITO,
                    INCOGNITO_STATE_PROVIDER, ACCESSIBILITY_ENABLED, MENU_IS_VISIBLE,
                    NEW_TAB_BUTTON_IS_VISIBLE, BUTTONS_CLICKABLE, IDENTITY_DISC_AT_START,
                    INCOGNITO_SWITCHER_VISIBLE, NEW_TAB_BUTTON_AT_START, IDENTITY_DISC_IS_VISIBLE,
                    IDENTITY_DISC_CLICK_HANDLER, IDENTITY_DISC_IMAGE, IDENTITY_DISC_DESCRIPTION,
                    IN_START_SURFACE_MODE, NEW_TAB_BUTTON_HIGHLIGHT, TRANSLATION_Y};
}
