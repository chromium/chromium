// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The view binder class for the auxiliary search module. */
public class AuxiliarySearchModuleViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        AuxiliarySearchModuleView moduleView = (AuxiliarySearchModuleView) view;
        if (AuxiliarySearchModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            moduleView.setModuleButtonOnClickListener(
                    model.get(AuxiliarySearchModuleProperties.MODULE_BUTTON_ON_CLICK_LISTENER));
        } else {
            assert false : "Unhandled property detected in AuxiliarySearchModuleViewBinder!";
        }
    }
}
