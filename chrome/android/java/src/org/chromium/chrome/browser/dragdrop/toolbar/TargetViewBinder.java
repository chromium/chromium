// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.widget.FrameLayout;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** View binder that binds the target view with the property model. */
class TargetViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, FrameLayout, PropertyKey> {
    @Override
    public void bind(PropertyModel model, FrameLayout view, PropertyKey propertyKey) {
        if (propertyKey == TargetViewProperties.TARGET_VIEW_VISIBLE) {
            view.setVisibility(model.get(TargetViewProperties.TARGET_VIEW_VISIBLE));
        } else if (propertyKey == TargetViewProperties.TARGET_VIEW_ACTIVE) {
            view.setActivated(model.get(TargetViewProperties.TARGET_VIEW_ACTIVE));
        } else if (propertyKey == TargetViewProperties.ON_DRAG_LISTENER) {
            view.setOnDragListener(model.get(TargetViewProperties.ON_DRAG_LISTENER));
        }
    }
}
