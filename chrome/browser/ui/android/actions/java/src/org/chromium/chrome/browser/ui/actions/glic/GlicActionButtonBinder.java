// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.widget.ImageView;

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
        if (propertyKey == GlicActionProperties.GLIC_DRAWABLE) {
            View targetView = ActionButtonBinder.resolveView(view);
            Drawable drawable = model.get(GlicActionProperties.GLIC_DRAWABLE);
            if (targetView instanceof ImageView imageView) {
                imageView.setImageDrawable(drawable);
                if (drawable instanceof LayerDrawable layerDrawable) {
                    if (layerDrawable.getNumberOfLayers() > 0) {
                        Drawable layer0 = layerDrawable.getDrawable(0);
                        if (layer0 instanceof Animatable animatable) {
                            animatable.start();
                        }
                    }
                }
            }
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }
}
