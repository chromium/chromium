// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties for the Safety Hub Magic Stack Module. */
interface SafetyHubMagicStackViewProperties {
    PropertyModel.WritableObjectPropertyKey<String> HEADER =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> SUMMARY =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<Drawable> ICON_DRAWABLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<String> BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    PropertyModel.WritableObjectPropertyKey<OnClickListener> BUTTON_ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                HEADER, TITLE, SUMMARY, ICON_DRAWABLE, BUTTON_TEXT, BUTTON_ON_CLICK_LISTENER,
            };
}
