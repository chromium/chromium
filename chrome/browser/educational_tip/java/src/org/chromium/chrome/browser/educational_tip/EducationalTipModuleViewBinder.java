// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the educational tip module. */
public class EducationalTipModuleViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        EducationalTipModuleView moduleView = (EducationalTipModuleView) view;
        if (EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING == propertyKey) {
            moduleView.setContentTitle(
                    model.get(EducationalTipModuleProperties.MODULE_CONTENT_TITLE_STRING));
        } else if (EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING
                == propertyKey) {
            moduleView.setContentDescription(
                    model.get(EducationalTipModuleProperties.MODULE_CONTENT_DESCRIPTION_STRING));
        } else if (EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            moduleView.setModuleButtonOnClickListener(
                    model.get(EducationalTipModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER));
        } else if (EducationalTipModuleProperties.MODULE_CONTENT_IMAGE == propertyKey) {
            moduleView.setContentImageResource(
                    model.get(EducationalTipModuleProperties.MODULE_CONTENT_IMAGE));
        } else {
            assert false : "Unhandled property detected in EducationalTipModuleViewBinder!";
        }
    }
}
