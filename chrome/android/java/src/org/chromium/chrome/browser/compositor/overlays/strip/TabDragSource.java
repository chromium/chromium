// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.DragEvent;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Log;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A singleton class manages initiating tab drag and drop and handles the events that are received
 * during drag and drop process. The tab drag and drop is initiated from the active instance of
 * {@link StripLayoutHelper}.
 */
public class TabDragSource {
    private static final String TAG = "TabDragSource";

    // TODO(b/285585036): Expand the ClipData definition to support dropping of the Tab info to be
    // used by SysUI that can parse this format.
    public static final String MIMETYPE_CHROME_TAB = "cr_tab/*";
    public static final String[] SUPPORTED_MIMETYPES = new String[] {MIMETYPE_CHROME_TAB};

    private static TabDragSource sTabDragSource;

    private MultiInstanceManager mMultiInstanceManager;
    private StripLayoutHelper mSourceStripLayoutHelper;
    private int mDragSourceTabsToolbarHashCode;
    private Tab mTabBeingDragged;
    private boolean mAcceptNextDrop;
    private OnDragListenerImpl mOnDragListenerImpl;
    private View.DragShadowBuilder mTabDragShadowBuilder;

    private float mPxToDp;

    private TabDragSource() {}

    public static TabDragSource getInstance() {
        if (sTabDragSource == null) {
            sTabDragSource = new TabDragSource();
        }
        return sTabDragSource;
    }

    int getDragSourceTabsToolbarHashCode() {
        return mDragSourceTabsToolbarHashCode;
    }

    void clearDragSourceTabsToolbarHashCode() {
        mDragSourceTabsToolbarHashCode = 0;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setDragSourceTabsToolbarHashCode(int tabsToolbarHashCode) {
        mDragSourceTabsToolbarHashCode = tabsToolbarHashCode;
    }

    Tab getTabBeingDragged() {
        return mTabBeingDragged;
    }

    void clearTabBeingDragged() {
        mTabBeingDragged = null;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setTabBeingDragged(Tab tabBeingDragged) {
        mTabBeingDragged = tabBeingDragged;
    }

    boolean getAcceptNextDrop() {
        return mAcceptNextDrop;
    }

    void clearAcceptNextDrop() {
        mAcceptNextDrop = false;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setAcceptNextDrop(boolean acceptNextDrop) {
        mAcceptNextDrop = acceptNextDrop;
    }

    MultiInstanceManager getMultiInstanceManager() {
        return mMultiInstanceManager;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void setMultiInstanceManager(MultiInstanceManager multiInstanceManager) {
        mMultiInstanceManager = multiInstanceManager;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    View.DragShadowBuilder getTabDragShadowBuilder() {
        return mTabDragShadowBuilder;
    }

    @VisibleForTesting
    class OnDragListenerImpl implements View.OnDragListener {
        private int mLastAction;
        private float mStartXPosition;
        private float mStartYPosition;
        private float mLastXPosition;
        private float mLastYPosition;
        private boolean mPointerInView;

        @Override
        public boolean onDrag(View view, DragEvent dragEvent) {
            // Check if the events are being received in the possible drop targets.
            if (canAcceptTabDrop(view, dragEvent)) return false;

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
                    if (mTabBeingDragged != null && mLastAction == DragEvent.ACTION_DRAG_EXITED) {
                        // Following call is device specific and is intended for specific platform
                        // SysUI.
                        sendPositionInfoToSysUI(view, mStartXPosition / mPxToDp,
                                mStartYPosition / mPxToDp, dragEvent.getX(), dragEvent.getY());

                        // Hence move the tab to a new Chrome window.
                        openTabInNewWindow();
                    }

                    // Clear the source view handle as DragNDrop is completed.
                    clearDragSourceTabsToolbarHashCode();
                    mSourceStripLayoutHelper.clearActiveClickedTab();
                    mTabDragShadowBuilder = null;
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
    }

    @VisibleForTesting
    void openTabInNewWindow() {
        if (mTabBeingDragged != null) {
            mMultiInstanceManager.moveTabToNewWindow(mTabBeingDragged);
            mTabBeingDragged = null;
        }
    }

    private void initiateTabDragAndDrop(View view, ClipData dragData, PointF startPointInView) {
        // Instantiate the drag shadow builder.
        mTabDragShadowBuilder = new TabDragShadowBuilder(
                getDragShadowView(view), getPositionOnScreen(view, startPointInView));

        // Start the drag.
        int flags = View.DRAG_FLAG_GLOBAL;
        view.startDragAndDrop(dragData, // The data to be dragged.
                mTabDragShadowBuilder, // The drag shadow builder.
                null, // No need to use local data.
                flags);
    }

    private static class TabDragShadowBuilder extends View.DragShadowBuilder {
        private PointF mStartPositionOnScreen;

        public TabDragShadowBuilder(View view, PointF startPositionOnScreen) {
            // Store the View parameter.
            super(view);
            mStartPositionOnScreen = startPositionOnScreen;
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
            touch.set(Math.round(mStartPositionOnScreen.x), Math.round(mStartPositionOnScreen.y));
            Log.d(TAG, "DnD onProvideShadowMetrics: " + mStartPositionOnScreen);
        }
    }

    private View getDragShadowView(View view) {
        Context context = view.getContext();
        ImageView imageView = new ImageView(context);
        try {
            Drawable icon =
                    context.getPackageManager().getApplicationIcon(context.getApplicationInfo());
            imageView.setImageDrawable(icon);
            imageView.setVisibility(View.VISIBLE);
            imageView.setBackgroundDrawable(new ColorDrawable(Color.LTGRAY));

            // Get Chrome window dimensions.
            int shadowWidth = ((Activity) context).getWindow().getDecorView().getWidth();
            int shadowHeight = ((Activity) context).getWindow().getDecorView().getHeight();
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
        return imageView;
    }

    @VisibleForTesting
    String getClipDataInfo(Tab clickedTab) {
        String tabData = "";
        if (clickedTab != null) {
            tabData = "TabId=" + clickedTab.getId();
        }

        return tabData;
    }

    /* Starts the tab drag action by initiating the process by calling @{link
     * View.startDragAndDrop}.
     *
     * @param tabsToolbarView @{link View} used to create the drag shadow.
     * @param sourceStripLayoutHelper @{link MultiInstanceManager} to forward drag message to @{link
     *         StripLayoutHelper} for reodering tabs.
     * @param tabBeingDragged @{link Tab} is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     */
    public boolean startTabDragAction(View tabsToolbarView,
            StripLayoutHelper sourceStripLayoutHelper, Tab tabBeingDragged, PointF startPoint) {
        if (!ChromeFeatureList.sTabDragDropAndroid.isEnabled()) return false;
        if (getDragSourceTabsToolbarHashCode() != 0) return false;

        assert (tabsToolbarView != null);
        assert (sourceStripLayoutHelper != null);

        mSourceStripLayoutHelper = sourceStripLayoutHelper;

        // Build the ClipData before initiating the drag action.
        ClipData tabClipData = createClipDataFromTab(tabBeingDragged, tabsToolbarView);
        if (tabClipData != null) {
            mTabBeingDragged = tabBeingDragged;
            initiateTabDragAndDrop(tabsToolbarView, tabClipData, startPoint);

            // Save this View handle to identify the source view.
            mDragSourceTabsToolbarHashCode = System.identityHashCode(tabsToolbarView);
            return true;
        }
        return false;
    }

    /* Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param tabsToolbarView @{link View} used to setup the drag and drop @{link
     *         View.OnDragListener}.
     * @param multiInstanceManager @{link MultiInstanceManager} to perform move action when drop
     *         completes.
     * @param tabDropTarget @{link TabDropTarget} used to receive the tab data drop from other
     *         Chrome windows via @{link ClipData}.
     */
    public void prepareForDragDrop(View tabsToolbarView, MultiInstanceManager multiInstanceManager,
            TabDropTarget tabDropTarget) {
        if (!ChromeFeatureList.sTabDragDropAndroid.isEnabled()) return;

        assert (tabsToolbarView != null);
        assert (multiInstanceManager != null);

        // Setup the environment.
        mPxToDp = 1.f / tabsToolbarView.getContext().getResources().getDisplayMetrics().density;
        mMultiInstanceManager = multiInstanceManager;

        // Setup a drop target and register the callback where the drag events
        // will be received.
        ViewCompat.setOnReceiveContentListener(
                tabsToolbarView, SUPPORTED_MIMETYPES, tabDropTarget.getDropContentReceiver());
        mOnDragListenerImpl = new OnDragListenerImpl();
        tabsToolbarView.setOnDragListener(mOnDragListenerImpl);
    }

    @VisibleForTesting
    float getPxToDp() {
        return mPxToDp;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    OnDragListenerImpl getOnDragListenerImpl() {
        return mOnDragListenerImpl;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    void resetTabDragSource() {
        // Reset all class variables.
        mMultiInstanceManager = null;
        mSourceStripLayoutHelper = null;
        mDragSourceTabsToolbarHashCode = 0;
        mTabBeingDragged = null;
        mOnDragListenerImpl = null;
        mPxToDp = 0.0f;
    }

    @VisibleForTesting
    boolean canAcceptTabDrop(View view, DragEvent dragEvent) {
        // If the event is received by a non source chrome window then mark to accept the drop in
        // the destination chrome window only if drop is within the tabs strip area.
        if (System.identityHashCode(view) != getDragSourceTabsToolbarHashCode()) {
            // Check if the drop is in the tabs strip area of the toolbar container.
            // The container has two toolbars strips stacked on each other. The top one is the tabs
            // strip layout and lower is for omnibox and navigation buttons. The tab drop is
            // accepted only in the top tabs toolbar area only.
            if ((dragEvent.getAction() == DragEvent.ACTION_DROP)
                    // TODO(b/288959915): Exactly determine the tabs toolbar area rather than using
                    // the approximate top half height of the toolbar container.
                    && (dragEvent.getY() < view.getHeight() / 2.0f)) {
                mAcceptNextDrop = true;
            }
            return true;
        }
        return false;
    }

    private ClipData createClipDataFromTab(Tab tabBeingDragged, View view) {
        String clipData = getClipDataInfo(tabBeingDragged);
        if (clipData != null && clipData.length() > 0) {
            // Create a new ClipData.Item from the ImageView object's tag.
            ClipData.Item item = new ClipData.Item((CharSequence) clipData);

            // Create a new ClipData using the tag as a label, the plain text MIME type,
            // and the already-created item. This creates a new ClipDescription object
            // within the ClipData and sets its MIME type to "cr_tab/plain".
            return new ClipData((CharSequence) view.getTag(), SUPPORTED_MIMETYPES, item);
        }
        return null;
    }

    int getTabIdFromClipData(ClipData.Item item) {
        String[] itemTexts = item.getText().toString().split(";");
        return Integer.parseInt(itemTexts[0].replaceAll("[^0-9]", ""));
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
}
