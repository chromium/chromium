// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipData;
import android.graphics.PointF;
import android.util.Pair;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ContentInfoCompat;
import androidx.core.view.OnReceiveContentListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.LocalizationUtils;

/**
 * The class manages receiving and handling the ClipData containing the Chrome Tab information
 * during the drag and drop process. It provides the callback for registration of the content
 * listener for the Toolbar container view.
 */
class TabDropTarget {
    private static final String TAG = "TabDropTarget";

    private final DropContentReceiver mDropContentReceiver;
    private StripLayoutHelper mDestinationStripLayoutHelper;

    TabDropTarget(StripLayoutHelper stripLayoutHelper) {
        mDropContentReceiver = new DropContentReceiver();
        mDestinationStripLayoutHelper = stripLayoutHelper;
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
        public @Nullable ContentInfoCompat onReceiveContent(View view, ContentInfoCompat payload) {
            if (!ChromeFeatureList.sTabDragDropAndroid.isEnabled()) return payload;
            if (payload == null) return payload;

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
                        // dragged. Return the orginal payload drop for next in line to receive the
                        // drop to handle.
                        if (sourceTabId != tabBeingDragged.getId()) {
                            Log.w(TAG, "DnD: Received an invalid tab drop.");
                            return payload;
                        }
                        int tabPositionIndex = getTabPositionIndex();
                        // TODO(b/290648035): Pass the Activity explicitly in place of casting the
                        // context handle.
                        tabDragSource.getMultiInstanceManager().moveTabToWindow(
                                (Activity) view.getContext(), tabBeingDragged, tabPositionIndex);
                        tabDragSource.clearTabBeingDragged();
                        tabDragSource.clearAcceptNextDrop();
                        mDestinationStripLayoutHelper.selectTabAtIndex(tabPositionIndex);
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

        private int getTabPositionIndex() {
            // Based on the location of the drop determine the position index where the tab will be
            // placed.
            PointF dropPosition = TabDragSource.getInstance().getTabDropPosition();
            StripLayoutTab droppedOn =
                    mDestinationStripLayoutHelper.getTabAtPosition(dropPosition.x);
            int tabPositionIndex = mDestinationStripLayoutHelper.getTabCount();
            // If not dropped on any existing tabs then simply add it at the end.
            if (droppedOn != null) {
                tabPositionIndex = mDestinationStripLayoutHelper.findIndexForTab(droppedOn.getId());
                // Check if the tab being moved needs to be added before or after the tab it was
                // dropped on based on the layout direction of tabs.
                float droppedTabCenterX = droppedOn.getDrawX() + droppedOn.getWidth() / 2.f;
                if (LocalizationUtils.isLayoutRtl()) {
                    if (dropPosition.x <= droppedTabCenterX) {
                        tabPositionIndex++;
                    }
                } else {
                    if (dropPosition.x > droppedTabCenterX) {
                        tabPositionIndex++;
                    }
                }
            }
            return tabPositionIndex;
        }
    }
}
