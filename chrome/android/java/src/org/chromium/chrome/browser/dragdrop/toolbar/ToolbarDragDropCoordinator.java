// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * ToolbarDragDrop Coordinator owns the view and the model change processor.
 */
public class ToolbarDragDropCoordinator implements OnDragListener {
    private static final String TEXT_MIME_TYPE = "text/plain";
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private FrameLayout mTargetView;
    private TargetViewDragListener mTargetViewDragListener;
    private PropertyModel mModel;

    /**
     * Create the Coordinator of TargetView that owns the view and the change process.
     *
     * @param targetView is the view displayed during the drag and drop process
     * @param autocompleteDelegate Used to navigate on a successful drop.
     */
    public ToolbarDragDropCoordinator(
            FrameLayout targetView, AutocompleteDelegate autocompleteDelegate) {
        // TargetView is inflated in order to have the onDragListener attached before it is visible
        mTargetView = targetView;
        mModel = new PropertyModel.Builder(TargetViewProperties.ALL_KEYS)
                         .with(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE)
                         .build();
        mTargetViewDragListener = new TargetViewDragListener(mModel, autocompleteDelegate);
        mModel.set(TargetViewProperties.ON_DRAG_LISTENER, mTargetViewDragListener);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mTargetView, new TargetViewBinder());
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                if (isValidMimeType(event)) {
                    mModel.set(TargetViewProperties.TARGET_VIEW_VISIBLE, View.VISIBLE);
                    return true;
                }
                return false;
            case DragEvent.ACTION_DRAG_ENDED:
                mModel.set(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE);
                // fall through
            default:
                return false;
        }
    }

    /**
     * Destroy the Toolbar Drag and Drop Coordinator and its components.
     */
    public void destroy() {
        mModel.set(TargetViewProperties.ON_DRAG_LISTENER, null);
        mTargetView.setVisibility(View.GONE);
        mModelChangeProcessor.destroy();
    }

    // Checks if the dragged object is supported for dragging into Omnibox
    public static boolean isValidMimeType(DragEvent event) {
        return event.getClipDescription().filterMimeTypes(TEXT_MIME_TYPE) != null;
    }
}
