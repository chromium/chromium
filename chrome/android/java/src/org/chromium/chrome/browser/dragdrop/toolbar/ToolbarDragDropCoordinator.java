// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop.toolbar;

import android.os.SystemClock;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * ToolbarDragDrop Coordinator owns the view and the model change processor.
 */
public class ToolbarDragDropCoordinator implements OnDragListener {
    // TODO(crbug.com/1469224): Swap error message to a translated string.
    private static final String ERROR_MESSAGE = "Unable to handle drop";
    private AutocompleteDelegate mAutocompleteDelegate;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private FrameLayout mTargetView;
    private TargetViewDragListener mTargetViewDragListener;
    private PropertyModel mModel;
    private OmniboxStub mOmniboxStub;

    interface OnDropCallback {
        /**
         * Handles parsing DragEvents on a drop to the Omnibox
         */
        void parseDragEvent(DragEvent event);
    }

    /**
     * Create the Coordinator of TargetView that owns the view and the change process.
     *
     * @param targetView is the view displayed during the drag and drop process.
     * @param autocompleteDelegate Used to navigate on a successful link drop.
     * @param omniboxStub Used to navigate a successful text drop.
     */
    public ToolbarDragDropCoordinator(FrameLayout targetView,
            AutocompleteDelegate autocompleteDelegate, OmniboxStub omniboxStub) {
        // TargetView is inflated in order to have the onDragListener attached before it is visible
        mTargetView = targetView;
        mModel = new PropertyModel.Builder(TargetViewProperties.ALL_KEYS)
                         .with(TargetViewProperties.TARGET_VIEW_VISIBLE, View.GONE)
                         .build();
        mAutocompleteDelegate = autocompleteDelegate;
        mTargetViewDragListener = new TargetViewDragListener(mModel, this::parseDragEvent);
        mModel.set(TargetViewProperties.ON_DRAG_LISTENER, mTargetViewDragListener);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mTargetView, new TargetViewBinder());
        mOmniboxStub = omniboxStub;
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
        return event.getClipDescription().filterMimeTypes(MimeTypeUtils.TEXT_MIME_TYPE) != null;
    }

    /**
     * Handles parsing DragEvents on a drop to the Omnibox.
     */
    @VisibleForTesting
    void parseDragEvent(DragEvent event) {
        if (event.getClipDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TEXT)) {
            mOmniboxStub.setUrlBarFocus(true,
                    event.getClipData()
                            .getItemAt(0)
                            .coerceToText(mTargetView.getContext())
                            .toString(),
                    OmniboxFocusReason.DRAG_DROP_TO_OMNIBOX);
        } else if (event.getClipDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_LINK)) {
            /**
             * This parsing is based on the implementation in
             * DragAndDropDelegateImpl#BuildClipData. Ideally we should handle build / parsing
             * in a similar place to keep things consistent.
             * TODO(crbug.com/1469084): Build ClipData and parse link URL using a static helper
             * method
             */
            String url = event.getClipData().getItemAt(0).getIntent().getData().toString();
            mAutocompleteDelegate.loadUrl(url, PageTransition.TYPED, SystemClock.uptimeMillis());
        } else {
            // case where dragged object is not from Chrome
            event.getClipDescription().filterMimeTypes(MimeTypeUtils.TEXT_MIME_TYPE);
            if (event.getClipData() == null) {
                Toast errorMessage =
                        Toast.makeText(mTargetView.getContext(), ERROR_MESSAGE, Toast.LENGTH_SHORT);
                errorMessage.show();
            } else {
                mOmniboxStub.setUrlBarFocus(true,
                        event.getClipData()
                                .getItemAt(0)
                                .coerceToText(mTargetView.getContext())
                                .toString(),
                        OmniboxFocusReason.DRAG_DROP_TO_OMNIBOX);
            }
        }
    }
}
