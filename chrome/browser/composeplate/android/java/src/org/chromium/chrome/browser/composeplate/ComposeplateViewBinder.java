// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.COLOR_STATE_LIST;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.INCOGNITO_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.IS_INCOGNITO_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.LENS_CLICK_LISTENER;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.TEXT_STYLE_RES_ID;
import static org.chromium.chrome.browser.composeplate.ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER;

import android.view.View;
import android.widget.ImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder class for the composeplate on the NTP. */
@NullMarked
public class ComposeplateViewBinder {
    public static void bind(PropertyModel model, ComposeplateView view, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (IS_INCOGNITO_BUTTON_VISIBLE == propertyKey) {
            View incognitoButton = view.findViewById(R.id.incognito_button);
            incognitoButton.setVisibility(
                    model.get(IS_INCOGNITO_BUTTON_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (VOICE_SEARCH_CLICK_LISTENER == propertyKey) {
            ImageView voiceSearchButton = view.findViewById(R.id.voice_search_button);
            if (voiceSearchButton != null) {
                voiceSearchButton.setOnClickListener(model.get(VOICE_SEARCH_CLICK_LISTENER));
            }
        } else if (LENS_CLICK_LISTENER == propertyKey) {
            ImageView lensButton = view.findViewById(R.id.lens_camera_button);
            if (lensButton != null) {
                lensButton.setOnClickListener(model.get(LENS_CLICK_LISTENER));
            }
        } else if (INCOGNITO_CLICK_LISTENER == propertyKey) {
            View incognitoButton = view.findViewById(R.id.incognito_button);
            incognitoButton.setOnClickListener(model.get(INCOGNITO_CLICK_LISTENER));
        } else if (COMPOSEPLATE_BUTTON_CLICK_LISTENER == propertyKey) {
            View composeplateButton = view.findViewById(R.id.composeplate_button);
            if (composeplateButton != null) {
                composeplateButton.setOnClickListener(
                        model.get(COMPOSEPLATE_BUTTON_CLICK_LISTENER));
            }
        } else if (APPLY_WHITE_BACKGROUND_WITH_SHADOW == propertyKey) {
            view.applyWhiteBackgroundWithShadow(model.get(APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        } else if (COLOR_STATE_LIST == propertyKey) {
            view.setColorStateList(model.get(COLOR_STATE_LIST));
        } else if (TEXT_STYLE_RES_ID == propertyKey) {
            view.setTextStyle(model.get(TEXT_STYLE_RES_ID));
        } else {
            assert false : "Unhandled property detected in ComposeplateViewBinder!";
        }
    }
}
