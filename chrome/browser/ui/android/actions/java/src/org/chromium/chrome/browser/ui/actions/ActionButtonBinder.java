// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.widget.ImageView;

import androidx.appcompat.widget.TooltipCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.util.TextResolver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for {@link ActionProperties}. */
@NullMarked
public class ActionButtonBinder {
    /**
     * Binds the given {@link PropertyModel} to the given {@link View} for the given {@link
     * PropertyKey}.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        View targetView = resolveView(view);
        if (ActionProperties.ICON_ID == propertyKey
                || ActionProperties.ICON_DRAWABLE == propertyKey) {
            Drawable drawable = model.get(ActionProperties.ICON_DRAWABLE);
            if (drawable != null) {
                if (targetView instanceof ImageView imageView) imageView.setImageDrawable(drawable);
            } else {
                int resId = model.get(ActionProperties.ICON_ID);
                if (targetView instanceof ImageView imageView) imageView.setImageResource(resId);
            }
        } else if (ActionProperties.CONTENT_DESCRIPTION_RESOLVER == propertyKey) {
            TextResolver resolver = model.get(ActionProperties.CONTENT_DESCRIPTION_RESOLVER);
            targetView.setContentDescription(
                    resolver != null ? resolver.resolve(view.getContext()) : "");
        } else if (ActionProperties.TOOLTIP_TEXT_RESOLVER == propertyKey) {
            TextResolver resolver = model.get(ActionProperties.TOOLTIP_TEXT_RESOLVER);
            TooltipCompat.setTooltipText(
                    targetView, resolver != null ? resolver.resolve(view.getContext()) : null);
        } else if (ActionProperties.ON_PRESS_CALLBACK == propertyKey
                || ActionProperties.BUTTON_STATE == propertyKey) {
            Callback<View> callback = model.get(ActionProperties.ON_PRESS_CALLBACK);
            boolean hasPressCallback = callback != null;
            targetView.setOnClickListener(hasPressCallback ? callback::onResult : null);
            @ButtonState
            int buttonState =
                    model.containsKey(ActionProperties.BUTTON_STATE)
                            ? model.get(ActionProperties.BUTTON_STATE)
                            : ButtonState.DEFAULT;
            ActionUtils.applyButtonState(targetView, buttonState, hasPressCallback);
        } else if (ActionProperties.ON_LONG_PRESS_CALLBACK == propertyKey) {
            Callback<View> callback = model.get(ActionProperties.ON_LONG_PRESS_CALLBACK);
            OnLongClickListener listener = null;
            if (callback != null) {
                listener =
                        v -> {
                            callback.onResult(v);
                            return true;
                        };
            }
            targetView.setOnLongClickListener(listener);
        } else if (ActionProperties.IPH_INTENT == propertyKey) {
            IphIntent iphIntent = model.get(ActionProperties.IPH_INTENT);
            if (iphIntent == null) return;
            UserEducationHelper userEducationHelper =
                    model.get(ActionProperties.USER_EDUCATION_HELPER);
            assert userEducationHelper != null : "UserEducationHelper is unset";
            if (targetView.isLaidOut()) {
                iphIntent.tryShow(targetView, userEducationHelper);
            } else {
                targetView.post(() -> iphIntent.tryShow(targetView, userEducationHelper));
            }
        }
    }

    /** Resolves the target view if the given view is a {@link DelegatingActionView}. */
    public static View resolveView(View view) {
        while (view instanceof DelegatingActionView delegatingView) {
            View target = delegatingView.getTargetView();
            if (target == view) break;
            view = target;
        }
        return view;
    }
}
