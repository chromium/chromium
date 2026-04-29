// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the Glic action state to a View. */
@NullMarked
public class GlicActionButtonBinder {
    /**
     * Binds the given {@link PropertyModel} to the given {@link View} for the given {@link
     * PropertyKey}.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        view = ActionButtonBinder.resolveView(view);

        if (propertyKey == GlicActionProperties.GLIC_STATE) {
            // TODO(crbug.com/491509952): Implement state specific rendering (Lottie, chips, etc.)
            // based on GlicToolbarButtonController behavior.
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }
}
