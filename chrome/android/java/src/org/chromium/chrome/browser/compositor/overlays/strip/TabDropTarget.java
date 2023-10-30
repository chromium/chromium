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
import androidx.core.view.ContentInfoCompat;
import androidx.core.view.OnReceiveContentListener;
import androidx.core.view.ViewCompat;

import org.chromium.base.Log;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.base.LocalizationUtils;

/**
 * The class manages receiving and handling the ClipData containing the Chrome Tab information
 * during the drag and drop process. It provides the callback for registration of the content
 * listener for the Toolbar container view.
 */
class TabDropTarget implements OnReceiveContentListener {
    private static final String TAG = "TabDropTarget";
    private final MultiInstanceManager mMultiInstanceManager;
    private final StripLayoutHelperManager mStripLayoutHelperManager;

    TabDropTarget(
            StripLayoutHelperManager stripLayoutHelperManager,
            MultiInstanceManager multiInstanceManager,
            View toolbarContainerView) {
        mStripLayoutHelperManager = stripLayoutHelperManager;
        mMultiInstanceManager = multiInstanceManager;
        // Setup a drop target and register the callback where the drag events
        // will be received.
        ViewCompat.setOnReceiveContentListener(
                toolbarContainerView,
                new String[] {ChromeDragAndDropBrowserDelegate.CHROME_MIMETYPE_TAB},
                this);
    }

    @Override
    public @Nullable ContentInfoCompat onReceiveContent(View view, ContentInfoCompat payload) {
        if (!TabUiFeatureUtilities.isTabDragEnabled()) return payload;
        if (payload == null) return payload;

        // Accept the drop to handle only if all the following conditions are met:
        // 1. Tab Toolbar view is from a different Chrome window/instance
        // 2. Tab being dragged is present
        // 3. The item being dropped in the tabs strip area hence marked as accepted.
        if (!isDragSource() && DragDropGlobalState.getInstance().acceptNextDrop) {
            Pair<ContentInfoCompat, ContentInfoCompat> split =
                    payload.partition(item -> item.getText() != null);
            ContentInfoCompat uriContent = split.first;
            ContentInfoCompat remainingContent = split.second;

            if (uriContent != null) {
                ClipData clip = uriContent.getClip();
                for (int i = 0; i < clip.getItemCount(); i++) {
                    int sourceTabId = getTabIdFromClipData(clip.getItemAt(i));
                    // Ignore the drop if the dropped tab id does not match the id of tab being
                    // dragged. Return the original payload drop for next in line to receive the
                    // drop to handle.
                    Tab tabBeingDragged = DragDropGlobalState.getInstance().tabBeingDragged;
                    if (tabBeingDragged == null || sourceTabId != tabBeingDragged.getId()) {
                        Log.w(TAG, "DnD: Received an invalid tab drop.");
                        return payload;
                    }
                    int tabPositionIndex = getTabPositionIndex();
                    // TODO(b/290648035): Pass the Activity explicitly in place of casting the
                    // context handle.
                    mMultiInstanceManager.moveTabToWindow(
                            (Activity) view.getContext(), tabBeingDragged, tabPositionIndex);
                    mStripLayoutHelperManager
                            .getActiveStripLayoutHelper()
                            .selectTabAtIndex(tabPositionIndex);
                    DragDropGlobalState.getInstance().reset();
                }
            }

            // Return anything that we didn't handle ourselves. This preserves the default
            // platform behavior for text and anything else for which we are not implementing
            // custom handling.
            return remainingContent;
        } else {
            Log.w(TAG, "DnD: Received a drop but ignored the payload.");
        }

        return payload;
    }

    private boolean isDragSource() {
        return DragDropGlobalState.getInstance().dragSourceInstanceId
                == mMultiInstanceManager.getCurrentInstanceId();
    }

    int getTabIdFromClipData(ClipData.Item item) {
        // TODO(b/285585036): Expand the ClipData definition to support dropping of the Tab info to
        // be used by SysUI that can parse this format.
        String[] itemTexts = item.getText().toString().split(";");
        String numberText = itemTexts[0].replaceAll("[^0-9]", "");
        return numberText.isEmpty() ? Tab.INVALID_TAB_ID : Integer.parseInt(numberText);
    }

    private int getTabPositionIndex() {
        // Based on the location of the drop determine the position index where the tab will be
        // placed.
        PointF dropPosition = DragDropGlobalState.getInstance().dropLocation;
        StripLayoutHelper activeStripHelper =
                mStripLayoutHelperManager.getActiveStripLayoutHelper();
        StripLayoutTab droppedOn = activeStripHelper.getTabAtPosition(dropPosition.x);
        int tabPositionIndex = activeStripHelper.getTabCount();
        // If not dropped on any existing tabs then simply add it at the end.
        if (droppedOn != null) {
            tabPositionIndex = activeStripHelper.findIndexForTab(droppedOn.getId());
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
