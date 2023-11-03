// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DropDataAndroid;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * StripLayoutHelper}.
 */
public class TabDragSource {
    private static final String TAG = "TabDragSource";

    private MultiInstanceManager mMultiInstanceManager;

    private DragAndDropDelegate mDragAndDropDelegate;
    private StripLayoutHelper mSourceStripLayoutHelper;
    private OnDragListenerImpl mOnDragListenerImpl;
    private PointF mDragShadowOffset = new PointF(0, 0);
    private float mTabStripHeightDp;
    private float mPxToDp;
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private View mDragSourceView;

    /**
     * Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param toolbarContainerView @{link View} used to setup the drag and drop @{link
     *     View.OnDragListener}.
     * @param multiInstanceManager @{link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate @{@link DragAndDropDelegate} to initiate tab drag and drop.
     * @param browserControlStateProvider @{@link BrowserControlsStateProvider} to compute
     *     drag-shadow dimens.
     */
    public TabDragSource(
            @NonNull View toolbarContainerView,
            @NonNull MultiInstanceManager multiInstanceManager,
            @NonNull DragAndDropDelegate dragAndDropDelegate,
            @NonNull BrowserControlsStateProvider browserControlStateProvider) {
        mPxToDp =
                1.f / toolbarContainerView.getContext().getResources().getDisplayMetrics().density;
        mMultiInstanceManager = multiInstanceManager;
        mDragAndDropDelegate = dragAndDropDelegate;
        mBrowserControlStateProvider = browserControlStateProvider;
        // Save the tabs toolbar height as it occupies the top half of the toolbar container.
        mTabStripHeightDp =
                toolbarContainerView
                        .getContext()
                        .getResources()
                        .getDimension(R.dimen.tab_strip_height);

        mOnDragListenerImpl = new OnDragListenerImpl();
        toolbarContainerView.setOnDragListener(mOnDragListenerImpl);
    }

    public OnDragListenerImpl getDragListenerForTesting() {
        return mOnDragListenerImpl;
    }

    @VisibleForTesting
    class OnDragListenerImpl implements View.OnDragListener {
        private int mLastAction;
        private float mStartXPosition;
        private float mStartYPosition;
        private float mLastXPosition;
        private float mLastYPosition;
        private boolean mPointerInView;
        private boolean mDragShadowVisible;

        @Override
        public boolean onDrag(View view, DragEvent dragEvent) {
            // Check if the events are being received in the possible drop targets.
            if (canAcceptTabDrop(dragEvent)) return false;

            // Since drag events are over Chrome window hence set the appropriate drag shadow, if
            // required.
            setDragShadow(dragEvent);

            switch (dragEvent.getAction()) {
                case DragEvent.ACTION_DRAG_STARTED:
                    resetState();
                    mStartXPosition = dragEvent.getX() * mPxToDp;
                    mStartYPosition = dragEvent.getY() * mPxToDp;
                    mLastXPosition = mStartXPosition;
                    mLastYPosition = mStartYPosition;
                    break;
                case DragEvent.ACTION_DRAG_LOCATION:
                    float curXPos = dragEvent.getX() * mPxToDp;
                    float curYPos = dragEvent.getY() * mPxToDp;
                    // TODO(b/285590087): Enter Android drag mode until tab is torn vertically to
                    // prevent forwarding drag events back into SripLayoutHelper #drag,
                    // #onUpOrCancel, #onDownInternal, etc.
                    if (mPointerInView) {
                        mSourceStripLayoutHelper.drag(LayoutManagerImpl.time(), curXPos, curYPos,
                                curXPos - mLastXPosition, curYPos - mLastYPosition,
                                curXPos - mStartXPosition, curYPos - mStartYPosition);
                    }
                    mLastXPosition = curXPos;
                    mLastYPosition = curYPos;
                    break;
                case DragEvent.ACTION_DROP:
                    if (mPointerInView) {
                        mSourceStripLayoutHelper.onUpOrCancel(LayoutManagerImpl.time());
                        mPointerInView = false;
                    }
                    break;
                case DragEvent.ACTION_DRAG_ENDED:
                    // Check if anyone handled the dropped ClipData meaning that drop was beyond
                    // acceptable drop area.
                    if (DragDropGlobalState.getInstance().tabBeingDragged != null
                            && mLastAction == DragEvent.ACTION_DRAG_EXITED) {
                        // Following call is device specific and is intended for specific platform
                        // SysUI.
                        sendPositionInfoToSysUI(view, mStartXPosition / mPxToDp,
                                mStartYPosition / mPxToDp, dragEvent.getX(), dragEvent.getY());

                        // Hence move the tab to a new Chrome window.
                        openTabInNewWindow();
                    }

                    // Notify DragNDrop is completed.
                    DragDropGlobalState.getInstance().reset();
                    mSourceStripLayoutHelper.clearActiveClickedTab();
                    break;
                case DragEvent.ACTION_DRAG_ENTERED:
                    mSourceStripLayoutHelper.onDownInternal(
                            LayoutManagerImpl.time(), mLastXPosition, mLastYPosition, true, 0);
                    mPointerInView = true;
                    break;
                case DragEvent.ACTION_DRAG_EXITED:
                    mSourceStripLayoutHelper.onUpOrCancel(LayoutManagerImpl.time());
                    mPointerInView = false;
                    break;
                default:
                    break;
            }

            // Save the last drag situation to determine if the drop is outside toolbar view
            mLastAction = dragEvent.getAction();
            return false;
        }

        @VisibleForTesting
        void resetState() {
            // All the defined @{link DragEvent}.ACTION_* events are greater than zero hence the
            // initial setting for last action can be zero.
            mLastAction = 0;
            mStartXPosition = 0.0f;
            mStartYPosition = 0.0f;
            mLastXPosition = 0.0f;
            mLastYPosition = 0.0f;
        }

        @VisibleForTesting
        boolean canAcceptTabDrop(DragEvent dragEvent) {
            // If the event is received by a non source chrome window then mark to accept the drop
            // in the destination chrome window only if drop is within the tabs strip area.
            if (!isDragSource()) {
                // Check if the drop is in the tabs strip area of the toolbar container.
                // The container has two toolbars strips stacked on each other. The top one is the
                // tabs strip layout and lower is for omnibox and navigation buttons. The tab drop
                // is accepted only in the top tabs toolbar area only.
                if (dragEvent.getAction() == DragEvent.ACTION_DROP) {
                    DragDropGlobalState.getInstance().dropLocation =
                            new PointF(dragEvent.getX() * mPxToDp, dragEvent.getY() * mPxToDp);
                    if (inTabsToolbarArea(dragEvent)) {
                        DragDropGlobalState.getInstance().acceptNextDrop = true;
                    }
                }
                return true;
            }
            return false;
        }

        private void showDragShadow(boolean show) {
            assert mDragSourceView != null;
            DragShadowBuilder builder =
                    createTabDragShadowBuilder(mDragSourceView.getContext(), show);
            mDragSourceView.updateDragShadow(builder);
            mDragShadowVisible = show;
        }

        private void setDragShadow(DragEvent dragEvent) {
            // Only drag source can edit drag shadow.
            assert isDragSource();
            switch (dragEvent.getAction()) {
                case DragEvent.ACTION_DRAG_LOCATION:
                    boolean inTabsStripArea = inTabsToolbarArea(dragEvent);
                    if (inTabsStripArea && mDragShadowVisible) {
                        showDragShadow(false);
                    }
                    if (!inTabsStripArea && !mDragShadowVisible) {
                        showDragShadow(true);
                    }
                    break;
                case DragEvent.ACTION_DROP:
                    showDragShadow(false);
                    break;
                case DragEvent.ACTION_DRAG_ENDED:
                    showDragShadow(false);
                    break;
                case DragEvent.ACTION_DRAG_ENTERED:
                    // No action here as the location is not specified. During the first
                    // ACTION_DRAG_LOCATION event the correct drag shadow will be set.
                    mDragShadowVisible = true;
                    break;
                case DragEvent.ACTION_DRAG_EXITED:
                    showDragShadow(true);
                    break;
                default:
                    break;
            }
        }
    }

    private boolean isDragSource() {
        return DragDropGlobalState.getInstance().dragSourceInstanceId
                == mMultiInstanceManager.getCurrentInstanceId();
    }

    @VisibleForTesting
    void openTabInNewWindow() {
        Tab tabBeingDragged = DragDropGlobalState.getInstance().tabBeingDragged;
        assert tabBeingDragged != null;
        mMultiInstanceManager.moveTabToNewWindow(tabBeingDragged);
    }

    private static class TabDragShadowBuilder extends View.DragShadowBuilder {
        private PointF mDragShadowOffset;

        public TabDragShadowBuilder(View view, PointF dragShadowOffset) {
            // Store the View parameter.
            super(view);
            mDragShadowOffset = dragShadowOffset;
        }

        // Defines a callback that sends the drag shadow dimensions and touch point
        // back to the system.
        @Override
        public void onProvideShadowMetrics(Point size, Point touch) {
            // Set the width of the shadow to half the width of the original
            // View.
            int width = getView().getWidth();

            // Set the height of the shadow to half the height of the original
            // View.
            int height = getView().getHeight();

            // Set the size parameter's width and height values. These get back
            // to the system through the size parameter.
            size.set(width, height);

            // Set the touch point of the drag shadow to be user's hold/touch point within Chrome
            // Window.
            touch.set(Math.round(mDragShadowOffset.x), Math.round(mDragShadowOffset.y));
            Log.d(TAG, "DnD onProvideShadowMetrics: " + mDragShadowOffset);
        }
    }

    /* Starts the tab drag action by initiating the process by calling @{link
     * View.startDragAndDrop}.
     *
     * @param toolbarContainerView @{link View} used to create the drag shadow.
     * @param sourceStripLayoutHelper @{link MultiInstanceManager} to forward drag message to @{link
     *         StripLayoutHelper} for reodering tabs.
     * @param tabBeingDragged @{link Tab} is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     */
    public boolean startTabDragAction(
            View toolbarContainerView,
            StripLayoutHelper sourceStripLayoutHelper,
            Tab tabBeingDragged,
            PointF startPoint) {
        if (!TabUiFeatureUtilities.isTabDragEnabled()) return false;
        if (DragDropGlobalState.getInstance().dragSourceInstanceId
                        != MultiWindowUtils.INVALID_INSTANCE_ID
                || tabBeingDragged == null) return false;
        assert (toolbarContainerView != null);
        assert (sourceStripLayoutHelper != null);

        setGlobalState(tabBeingDragged);

        mSourceStripLayoutHelper = sourceStripLayoutHelper;
        mDragSourceView = toolbarContainerView;
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            mDragShadowOffset = getPositionOnScreen(toolbarContainerView, startPoint);
        }

        DropDataAndroid dropData =
                new ChromeDropDataAndroid.Builder().withTabId(tabBeingDragged.getId()).build();
        DragShadowBuilder builder =
                createTabDragShadowBuilder(toolbarContainerView.getContext(), false);
        return mDragAndDropDelegate.startDragAndDrop(toolbarContainerView, builder, dropData);
    }

    private void setGlobalState(Tab tabBeingDragged) {
        DragDropGlobalState.getInstance().tabBeingDragged = tabBeingDragged;
        DragDropGlobalState.getInstance().dragSourceInstanceId =
                mMultiInstanceManager.getCurrentInstanceId();
    }

    @NonNull
    @VisibleForTesting
    DragShadowBuilder createTabDragShadowBuilder(Context context, boolean show) {
        int shadowWidthPx;
        int shadowHeightPx;
        ImageView imageView = new ImageView(context);
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            // View is empty and nothing is shown for now.
            // Get Chrome window dimensions and set the view to that size.
            shadowWidthPx = ((Activity) context).getWindow().getDecorView().getWidth();
            shadowHeightPx = ((Activity) context).getWindow().getDecorView().getHeight();
            if (show) {
                addAppIconToShadow(imageView, context, shadowWidthPx, shadowHeightPx);
            }
        } else {
            shadowWidthPx =
                    context.getResources().getDimensionPixelSize(R.dimen.tab_hover_card_width);
            shadowHeightPx =
                    TabUtils.deriveGridCardHeight(
                            shadowWidthPx, context, mBrowserControlStateProvider);
        }
        if (show) {
            imageView.setBackgroundDrawable(new ColorDrawable(Color.LTGRAY));
        }
        imageView.layout(0, 0, shadowWidthPx, shadowHeightPx);
        return new TabDragShadowBuilder(imageView, mDragShadowOffset);
    }

    private void addAppIconToShadow(
            ImageView imageView, Context context, int shadowWidth, int shadowHeight) {
        try {
            Drawable icon =
                    context.getPackageManager().getApplicationIcon(context.getApplicationInfo());
            imageView.setImageDrawable(icon);

            // Add app icon in the center of the drag shadow.
            int iconWidth = icon.getIntrinsicWidth();
            int iconHeight = icon.getIntrinsicHeight();
            int paddingHorizontal = (shadowWidth - iconWidth) / 2;
            int paddingVertical = (shadowHeight - iconHeight) / 2;
            imageView.setPadding(
                    paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
            imageView.layout(0, 0, shadowWidth, shadowHeight);
        } catch (Exception e) {
            Log.e(TAG, "DnD Failed to create drag shadow image view: " + e.getMessage());
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void sendPositionInfoToSysUI(View view, float startXInView, float startYInView,
            float endXInScreen, float endYInScreen) {
        // The start position is in the view coordinate system and related to the top left position
        // of the toolbar container view. Convert it to the screen coordinate system for comparison
        // with the drop position which is in screen coordinates.
        int[] topLeftLocation = new int[2];
        view.getLocationOnScreen(topLeftLocation);
        float startXInScreen = topLeftLocation[0] + startXInView;
        float startYInScreen = topLeftLocation[1] + startYInView;

        Activity activity = (Activity) view.getContext();
        View decorView = activity.getWindow().getDecorView();

        // Compute relative offsets based on screen coords of the source window dimensions.
        // Tabet screen is:
        //          -------------------------------------
        //          |    Source                         |  Relative X Offset =
        //          |    window                         |    (x2 - x1) / width
        //          |   (x1, y1)                        |
        //       |->|   ---------                       |  Relative Y Offset =
        // height|  |   |   *   |                       |    (y2 - y1) / height
        //       |  |   |       |                       |
        //       |->|   ---------                       |
        //          |               ---------           |
        //          |               |   *   |           |
        //          |               |       |           |
        //          |               ---------           |
        //          |                (x2, y2)           |
        //          |              Destination          |
        //          -------------------------------------
        //              <------->
        //               width
        // * is touch point and the anchor of drag shadow of the window for the tab drag and drop.
        float xOffsetRelative2WindowWidth = (endXInScreen - startXInScreen) / decorView.getWidth();
        float yOffsetRelative2WindowHeight =
                (endYInScreen - startYInScreen) / decorView.getHeight();

        // Prepare the positioning intent for SysUI to place the next Chrome window.
        // The intent is ignored when not handled with no impact on existing Android platforms.
        Intent intent = new Intent();
        intent.setPackage("com.android.systemui");
        intent.setAction("com.android.systemui.CHROME_TAB_DRAG_DROP");
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_TASK_ID", activity.getTaskId());
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_OFFSET_X", xOffsetRelative2WindowWidth);
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_OFFSET_Y", yOffsetRelative2WindowHeight);
        activity.sendBroadcast(intent);
        Log.d(TAG,
                "DnD Position info for SysUI: tId=" + activity.getTaskId() + ", xOff="
                        + xOffsetRelative2WindowWidth + ", yOff=" + yOffsetRelative2WindowHeight);
    }

    private PointF getPositionOnScreen(View view, PointF positionInView) {
        int[] topLeftLocationOfToolbarView = new int[2];
        view.getLocationOnScreen(topLeftLocationOfToolbarView);

        int[] topLeftLocationOfDecorView = new int[2];
        View decorView = ((Activity) view.getContext()).getWindow().getDecorView();
        decorView.getLocationOnScreen(topLeftLocationOfDecorView);

        float positionXOnScreen = (topLeftLocationOfToolbarView[0] - topLeftLocationOfDecorView[0])
                + positionInView.x / mPxToDp;
        float positionYOnScreen = (topLeftLocationOfToolbarView[1] - topLeftLocationOfDecorView[1])
                + positionInView.y / mPxToDp;
        return new PointF(positionXOnScreen, positionYOnScreen);
    }

    private boolean inTabsToolbarArea(DragEvent dragEvent) {
        return (dragEvent.getY() <= mTabStripHeightDp);
    }
}
