// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipData;
import android.util.Pair;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ContentInfoCompat;
import androidx.core.view.OnReceiveContentListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;

/**
 * The class manages receiving and handling the ClipData containing the Chrome Tab information
 * during the drag and drop process. It provides the callback for registration of the content
 * listener for the Toolbar container view.
 */
class TabDropTarget {
    private static final String TAG = "TabDropTarget";

    private final DropContentReceiver mDropContentReceiver;

    TabDropTarget() {
        mDropContentReceiver = new DropContentReceiver();
    }

    /**
     * Return the handle to implementation of the OnReceiveContentListener for registration with the
     * Toolbar container view to listen to payload drops.
     */
    DropContentReceiver getDropContentReceiver() {
        return mDropContentReceiver;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    class DropContentReceiver implements OnReceiveContentListener {
        @Override
        public ContentInfoCompat onReceiveContent(View view, ContentInfoCompat payload) {
            // Accept the drop to handle only if all the following conditions are met:
            // 1. Tab Toolbar view is from a different Chrome window/instance
            // 2. Tab being dragged is present
            // 3. The item being dropped in the tabs strip area hence marked as accepted.
            TabDragSource tabDragSource = TabDragSource.getInstance();
            Tab tabBeingDragged = tabDragSource.getTabBeingDragged();
            if (tabDragSource.getDragSourceTabsToolbarHashCode() != System.identityHashCode(view)
                    && (tabBeingDragged != null) && tabDragSource.getAcceptNextDrop()) {
                Pair<ContentInfoCompat, ContentInfoCompat> split =
                        payload.partition(item -> item.getText() != null);
                ContentInfoCompat uriContent = split.first;
                ContentInfoCompat remainingContent = split.second;

                if (uriContent != null) {
                    ClipData clip = uriContent.getClip();
                    for (int i = 0; i < clip.getItemCount(); i++) {
                        int sourceTabId = tabDragSource.getTabIdFromClipData(clip.getItemAt(i));
                        // Ignore the drop if the dropped tab id does not match the id of tab being
                        // dragged. Return the original payload drop for next in line to receive the
                        // drop to handle.
                        if (sourceTabId != tabBeingDragged.getId()) {
                            Log.w(TAG, "DnD: Received an invalid tab drop.");
                            return payload;
                        }
                        // TODO(b/290648035): Pass the Activity explicitly in place of casting the
                        // context handle.
                        tabDragSource.getMultiInstanceManager().moveTabToWindow(
                                (Activity) view.getContext(), tabBeingDragged);
                        tabDragSource.clearTabBeingDragged();
                        tabDragSource.clearAcceptNextDrop();
                    }
                }

                // Clear the source view handle as DragNDrop is completed.
                tabDragSource.clearDragSourceTabsToolbarHashCode();

                // Return anything that we didn't handle ourselves. This preserves the default
                // platform behavior for text and anything else for which we are not implementing
                // custom handling.
                return remainingContent;
            } else {
                Log.w(TAG, "DnD: Received a drop but ignored the payload.");
            }

            return payload;
        }
    }
}
