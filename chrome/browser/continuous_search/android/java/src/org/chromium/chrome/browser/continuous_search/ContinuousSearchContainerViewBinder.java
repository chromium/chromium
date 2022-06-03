// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Responsible for binding the {@link PropertyModel} for the Continuous Search container to its
 * Android and composited CC UI.
 */
class ContinuousSearchContainerViewBinder {
    static void bindJavaView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ContinuousSearchContainerProperties.VERTICAL_OFFSET == propertyKey) {
            view.setY((float) model.get(ContinuousSearchContainerProperties.VERTICAL_OFFSET));
        } else if (ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY == propertyKey) {
            view.setVisibility(
                    model.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));
        }
    }

    static void bindCompositedView(
            PropertyModel model, ContinuousSearchSceneLayer sceneLayer, PropertyKey propertyKey) {
        sceneLayer.setVerticalOffset(
                model.get(ContinuousSearchContainerProperties.VERTICAL_OFFSET));
        sceneLayer.setIsVisible(
                model.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
    }
}
