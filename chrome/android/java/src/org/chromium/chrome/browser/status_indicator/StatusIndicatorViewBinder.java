// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class StatusIndicatorViewBinder {
    /**
     * A wrapper class that holds a {@link ViewResourceFrameLayout} and a composited layer to be
     * used with the {@link StatusIndicatorSceneLayer}.
     */
    static class ViewHolder {
        /** A handle to the Android View based version of the status indicator. */
        public final ViewResourceFrameLayout javaViewRoot;

        /** A handle to the composited status indicator layer. */
        public final StatusIndicatorSceneLayer sceneLayer;

        /**
         * @param root The Android View based status indicator.
         */
        public ViewHolder(ViewResourceFrameLayout root, StatusIndicatorSceneLayer overlay) {
            javaViewRoot = root;
            sceneLayer = overlay;
        }
    }

    static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (StatusIndicatorProperties.STATUS_TEXT == propertyKey) {
            ((TextView) view.javaViewRoot.findViewById(R.id.status_text))
                    .setText(model.get(StatusIndicatorProperties.STATUS_TEXT));
        } else if (StatusIndicatorProperties.STATUS_ICON == propertyKey) {
            final Drawable drawable = model.get(StatusIndicatorProperties.STATUS_ICON);
            ((TextView) view.javaViewRoot.findViewById(R.id.status_text))
                    .setCompoundDrawablesRelative(drawable, null, null, null);
        } else if (StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE == propertyKey) {
            assert view.sceneLayer != null;
            view.sceneLayer.setIsVisible(
                    model.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        } else if (StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY == propertyKey) {
            view.javaViewRoot.setVisibility(
                    model.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        } else if (StatusIndicatorProperties.BACKGROUND_COLOR == propertyKey) {
            view.javaViewRoot.setBackgroundColor(
                    model.get(StatusIndicatorProperties.BACKGROUND_COLOR));
        } else if (StatusIndicatorProperties.TEXT_ALPHA == propertyKey) {
            final View text = view.javaViewRoot.findViewById(R.id.status_text);
            text.setAlpha(model.get(StatusIndicatorProperties.TEXT_ALPHA));
        } else if (StatusIndicatorProperties.TEXT_COLOR == propertyKey) {
            final TextView text = view.javaViewRoot.findViewById(R.id.status_text);
            text.setTextColor(model.get(StatusIndicatorProperties.TEXT_COLOR));
        } else if (StatusIndicatorProperties.ICON_TINT == propertyKey) {
            final TextViewWithCompoundDrawables text =
                    view.javaViewRoot.findViewById(R.id.status_text);
            final ColorStateList tint =
                    ColorStateList.valueOf(model.get(StatusIndicatorProperties.ICON_TINT));
            text.setDrawableTintColor(tint);
        } else if (StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT == propertyKey) {
            final float yOffset =
                    model.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT)
                            - view.javaViewRoot.getHeight();
            view.javaViewRoot.setTranslationY(yOffset);
        } else if (StatusIndicatorProperties.IS_OBSCURED == propertyKey) {
            view.javaViewRoot.setImportantForAccessibility(
                    model.get(StatusIndicatorProperties.IS_OBSCURED)
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        } else {
            assert false : "Unhandled property detected in StatusIndicatorViewBinder!";
        }
    }
}
