// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import org.chromium.chrome.browser.dragdrop.toolbar.ToolbarDragDropCoordinator.OnDropCallback;
import org.chromium.ui.modelutil.PropertyModel;

/** A drag listener for the target view that handles events during drag and drop to Omnibox */
class TargetViewDragListener implements OnDragListener {
    private OnDropCallback mOnDropCallback;
    private PropertyModel mModel;

    /**
     * Create the drag listener for the target view.
     *
     * @param model {@link PropertyModel} built with {@link TargetViewProperties}
     * @param onDropCallback Used to navigate on a successful drop.
     */
    public TargetViewDragListener(PropertyModel model, OnDropCallback onDropCallback) {
        mModel = model;
        mOnDropCallback = onDropCallback;
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                return true;
            case DragEvent.ACTION_DRAG_ENTERED:
                mModel.set(TargetViewProperties.TARGET_VIEW_ACTIVE, true);
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                mModel.set(TargetViewProperties.TARGET_VIEW_ACTIVE, false);
                break;
            case DragEvent.ACTION_DROP:
                mOnDropCallback.parseDragEvent(event);
                mModel.set(TargetViewProperties.TARGET_VIEW_ACTIVE, false);
                return true;
        }
        return false;
    }
}
