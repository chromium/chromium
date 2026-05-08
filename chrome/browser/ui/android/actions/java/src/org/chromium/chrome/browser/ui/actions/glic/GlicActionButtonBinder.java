// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.glic.GlicUiHelper;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.R;
import org.chromium.chrome.browser.ui.actions.glic.GlicActionProperties.GlicState;
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
            int state = model.get(GlicActionProperties.GLIC_STATE);
            if (view instanceof ImageView imageView) {
                updateImageForState(imageView, state);
            }
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }

    private static void updateImageForState(ImageView imageView, int state) {
        // TODO(crbug.com/491509952): Update icons suggested specifically for Glic bottom bar
        // button.
        Context context = imageView.getContext();
        if (state == GlicState.WORKING) {
            int sparkResId = R.drawable.ic_spark_24dp;
            Drawable sparkIcon = AppCompatResources.getDrawable(context, sparkResId);
            Drawable drawable = GlicUiHelper.createWorkingDrawable(context, sparkIcon);
            imageView.setImageDrawable(drawable);
        } else if (state == GlicState.NEEDS_REVIEW || state == GlicState.DONE) {
            imageView.setImageResource(R.drawable.glic_dirty_dot_spark);
        } else {
            imageView.setImageResource(R.drawable.ic_spark_24dp);
        }
    }
}
