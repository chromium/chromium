// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Safety Hub Magic Stack module. */
class SafetyHubMagicStackViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        SafetyHubMagicStackView moduleView = (SafetyHubMagicStackView) view;
        if (SafetyHubMagicStackViewProperties.HEADER == propertyKey) {
            moduleView.setHeader(model.get(SafetyHubMagicStackViewProperties.HEADER));
        } else if (SafetyHubMagicStackViewProperties.TITLE == propertyKey) {
            moduleView.setTitle(model.get(SafetyHubMagicStackViewProperties.TITLE));
        } else if (SafetyHubMagicStackViewProperties.SUMMARY == propertyKey) {
            moduleView.setSummary(model.get(SafetyHubMagicStackViewProperties.SUMMARY));
        } else if (SafetyHubMagicStackViewProperties.ICON_DRAWABLE == propertyKey) {
            moduleView.setIconDrawable(model.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE));
        } else if (SafetyHubMagicStackViewProperties.BUTTON_TEXT == propertyKey) {
            moduleView.setButtonText(model.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT));
        } else if (SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER == propertyKey) {
            moduleView.setButtonOnClickListener(
                    model.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER));
        } else {
            assert false : "Unhandled property detected in SafetyHubMagicStackViewBinder";
        }
    }
}
