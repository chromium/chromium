// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.ViewResourceFrameLayout;
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
            ((TextView) view.javaViewRoot.findViewById(R.id.status_text))
                    .setCompoundDrawablesRelativeWithIntrinsicBounds(
                            model.get(StatusIndicatorProperties.STATUS_ICON), null, null, null);
        } else if (StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE == propertyKey) {
            assert view.sceneLayer != null;
            view.sceneLayer.setIsVisible(
                    model.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        } else if (StatusIndicatorProperties.ANDROID_VIEW_VISIBLE == propertyKey) {
            view.javaViewRoot.setVisibility(
                    model.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBLE) ? View.VISIBLE
                                                                              : View.GONE);
        } else {
            assert false : "Unhandled property detected in StatusIndicatorViewBinder!";
        }
    }
}
