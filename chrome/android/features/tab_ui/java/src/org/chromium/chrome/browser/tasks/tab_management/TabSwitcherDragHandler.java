// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.PointF;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DragAndDropDelegate;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * TabListCoordinator}.
 */
@NullMarked
public class TabSwitcherDragHandler extends TabDragHandlerBase {

    /** Allows to handle tab drag and drop events. */
    interface DragHandlerDelegate {
        default boolean handleDragStart(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragEnd(float xPx, float yPx) {
            return false;
        }

        default boolean handleDragEnter() {
            return false;
        }

        default boolean handleDragExit() {
            return false;
        }

        default boolean handleDragLocation(float xPx, float yPx) {
            return false;
        }

        default boolean handleDrop(float xPx, float yPx) {
            return false;
        }
    }

    private @Nullable DragHandlerDelegate mDragHandlerDelegate;

    /**
     * Prepares the tab container view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param activitySupplier Supplier for the current activity.
     * @param multiInstanceManager {@link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     */
    public TabSwitcherDragHandler(
            Supplier<@Nullable Activity> activitySupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            Supplier<Boolean> isAppInDesktopWindowSupplier) {
        super(
                activitySupplier,
                multiInstanceManager,
                dragAndDropDelegate,
                isAppInDesktopWindowSupplier);
    }

    /**
     * Sets an object to handle tab drag events.
     *
     * @param DragHandlerDelegate Instance of {@link DragHandlerDelegate}
     */
    public void setDragHandlerDelegate(DragHandlerDelegate dragHandlerDelegate) {
        mDragHandlerDelegate = dragHandlerDelegate;
    }

    /**
     * Starts the tab drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tab Tab is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     * @return true if the drag action was initiated successfully.
     */
    public boolean startTabDragAction(View dragSourceView, Tab tab, PointF startPoint) {
        if (!canStartTabDrag()) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareTabDropData(tab);
        return startDragInternal(dropData, startPoint, dragSourceView);
    }

    /**
     * Starts the group drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabGroupId The dragged group's ID.
     * @param startPoint Position of the drag start point in view coordinates.
     * @return {@code True} if the drag action was initiated successfully.
     */
    public boolean startGroupDragAction(View dragSourceView, Token tabGroupId, PointF startPoint) {
        if (!canStartGroupDrag(tabGroupId)) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareGroupDropData(tabGroupId, false);
        return startDragInternal(dropData, startPoint, dragSourceView);
    }

    private boolean startDragInternal(
            ChromeDropDataAndroid dropData, PointF startPoint, View dragSourceView) {
        DragShadowBuilder builder =
                new DragShadowBuilder(dragSourceView) {
                    @Override
                    public void onProvideShadowMetrics(Point shadowSize, Point shadowTouchPoint) {
                        View view = getView();
                        if (view != null) {
                            int width = view.getWidth();
                            int height = view.getHeight();
                            int touchX = (int) (startPoint.x - view.getX());
                            int touchY = (int) (startPoint.y - view.getY());

                            shadowSize.set(width, height);
                            shadowTouchPoint.set(touchX, touchY);
                        } else {
                            shadowSize.set(0, 0);
                            shadowTouchPoint.set(0, 0);
                        }
                    }
                };

        return startDrag(dragSourceView, builder, dropData);
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean res = false;

        // No-op if the handler delegate is missing.
        if (mDragHandlerDelegate == null) {
            return res;
        }

        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                if (isDraggingBrowserContent(dragEvent.getClipDescription())) {
                    res = mDragHandlerDelegate.handleDragStart(dragEvent.getX(), dragEvent.getY());
                }
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                finishDrag(dragEvent.getResult());
                res = mDragHandlerDelegate.handleDragEnd(dragEvent.getX(), dragEvent.getY());
                break;
            case DragEvent.ACTION_DRAG_ENTERED:
                res = mDragHandlerDelegate.handleDragEnter();
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                res = mDragHandlerDelegate.handleDragExit();
                break;
            case DragEvent.ACTION_DRAG_LOCATION:
                res = mDragHandlerDelegate.handleDragLocation(dragEvent.getX(), dragEvent.getY());
                break;
            case DragEvent.ACTION_DROP:
                res = mDragHandlerDelegate.handleDrop(dragEvent.getX(), dragEvent.getY());
                break;
        }
        return res;
    }
}
