// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.AI_MODE_BUTTON_ICON;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.AI_MODE_BUTTON_UI_CONFIG;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.APPLY_WHITE_BACKGROUND;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.COLOR_STATE_LIST;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.INCOGNITO_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.IS_OPTIONAL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.OPTIONAL_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.OPTIONAL_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.OPTIONAL_BUTTON_ICON_RES_ID;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.OPTIONAL_BUTTON_LATERAL_PADDING;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.OPTIONAL_BUTTON_MARGIN_END;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.TEXT_STYLE_RES_ID;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder class for the composeplate on the NTP. */
@NullMarked
public class ComposeplateViewBinder {
    public static void bind(PropertyModel model, ComposeplateView view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (INCOGNITO_CLICK_LISTENER == propertyKey) {
            View incognitoButton = view.findViewById(R.id.incognito_button);
            incognitoButton.setOnClickListener(model.get(INCOGNITO_CLICK_LISTENER));
        } else if (COMPOSEPLATE_BUTTON_CLICK_LISTENER == propertyKey) {
            View composeplateButton = view.findViewById(R.id.composeplate_button);
            if (composeplateButton != null) {
                composeplateButton.setOnClickListener(
                        model.get(COMPOSEPLATE_BUTTON_CLICK_LISTENER));
            }
        } else if (APPLY_WHITE_BACKGROUND == propertyKey) {
            view.applyWhiteBackground(model.get(APPLY_WHITE_BACKGROUND));
        } else if (COLOR_STATE_LIST == propertyKey) {
            view.setColorStateList(model.get(COLOR_STATE_LIST));
        } else if (TEXT_STYLE_RES_ID == propertyKey) {
            view.setTextStyle(model.get(TEXT_STYLE_RES_ID));
        } else if (AI_MODE_BUTTON_UI_CONFIG == propertyKey) {
            view.setAiModeButtonUiConfig(model.get(AI_MODE_BUTTON_UI_CONFIG));
        } else if (AI_MODE_BUTTON_ICON == propertyKey) {
            view.setAiModeButtonIcon(model.get(AI_MODE_BUTTON_ICON));
        } else if (IS_OPTIONAL_BUTTON_VISIBLE == propertyKey) {
            view.setOptionalButtonVisibility(model.get(IS_OPTIONAL_BUTTON_VISIBLE));
        } else if (OPTIONAL_BUTTON_ICON_RES_ID == propertyKey) {
            view.setOptionalButtonIcon(model.get(OPTIONAL_BUTTON_ICON_RES_ID));
        } else if (OPTIONAL_BUTTON_CLICK_LISTENER == propertyKey) {
            view.setOptionalButtonClickListener(model.get(OPTIONAL_BUTTON_CLICK_LISTENER));
        } else if (OPTIONAL_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setOptionalButtonContentDescription(
                    model.get(OPTIONAL_BUTTON_CONTENT_DESCRIPTION));
        } else if (OPTIONAL_BUTTON_LATERAL_PADDING == propertyKey) {
            view.setButtonLateralPadding(model.get(OPTIONAL_BUTTON_LATERAL_PADDING));
        } else if (OPTIONAL_BUTTON_MARGIN_END == propertyKey) {
            view.setButtonMarginEnd(model.get(OPTIONAL_BUTTON_MARGIN_END));
        } else {
            assert false : "Unhandled property detected in ComposeplateViewBinder!";
        }
    }
}
