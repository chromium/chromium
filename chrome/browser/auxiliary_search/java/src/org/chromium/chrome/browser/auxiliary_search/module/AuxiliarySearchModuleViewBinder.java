// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder class for the auxiliary search module. */
@NullMarked
public class AuxiliarySearchModuleViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        AuxiliarySearchModuleView moduleView = (AuxiliarySearchModuleView) view;
        if (AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            moduleView.setFirstButtonOnClickListener(
                    model.get(
                            AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_ON_CLICK_LISTENER));
        } else if (AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_ON_CLICK_LISTENER
                == propertyKey) {
            moduleView.setSecondButtonOnClickListener(
                    model.get(
                            AuxiliarySearchModuleProperties
                                    .MODULE_SECOND_BUTTON_ON_CLICK_LISTENER));
        } else if (AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID == propertyKey) {
            moduleView.setContentTextResId(
                    model.get(AuxiliarySearchModuleProperties.MODULE_CONTENT_TEXT_RES_ID));
        } else if (AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID == propertyKey) {
            moduleView.setFirstButtonTextResId(
                    model.get(AuxiliarySearchModuleProperties.MODULE_FIRST_BUTTON_TEXT_RES_ID));
        } else if (AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID
                == propertyKey) {
            moduleView.setSecondButtonTextResId(
                    model.get(AuxiliarySearchModuleProperties.MODULE_SECOND_BUTTON_TEXT_RES_ID));
        } else {
            assert false : "Unhandled property detected in AuxiliarySearchModuleViewBinder!";
        }
    }
}
