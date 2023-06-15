// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.DragEvent;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ContentInfoCompat;
import androidx.core.view.OnReceiveContentListener;
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
    private DropContentReceiver mDropContentReceiver;
    private OnDragListenerImpl mOnDragListenerImpl;

    private float mPxToDp;

    private TabDragSource() {}

    public static TabDragSource getInstance() {
        if (sTabDragSource == null) {
            sTabDragSource = new TabDragSource();
        }
        return sTabDragSource;
    }

    public int getDragSourceTabsToolbarHashCode() {
        return mDragSourceTabsToolbarHashCode;
    }

    private void setDragSourceTabsToolbarHashCode(int tabsToolbarHashCode) {
        mDragSourceTabsToolbarHashCode = tabsToolbarHashCode;
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
                        // Hence move the tab to a new Chrome window.
                        openTabInNewWindow();
                    }

                    // Clear the source view handle as DragNDrop is completed.
                    setDragSourceTabsToolbarHashCode(0);
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
    }

    @VisibleForTesting
    void openTabInNewWindow() {
        if (mTabBeingDragged != null) {
            mMultiInstanceManager.moveTabToNewWindow(mTabBeingDragged);
            mTabBeingDragged = null;
        }
    }

    private void initiateTabDragAndDrop(View view, String clipData) {
        // Create a new ClipData.Item from the ImageView object's tag.
        ClipData.Item item = new ClipData.Item((CharSequence) clipData);

        // Create a new ClipData using the tag as a label, the plain text MIME type,
        // and the already-created item. This creates a new ClipDescription object
        // within the ClipData and sets its MIME type to "cr_tab/plain".
        ClipData dragData = new ClipData((CharSequence) view.getTag(), SUPPORTED_MIMETYPES, item);

        // Instantiate the drag shadow builder.
        View.DragShadowBuilder tabShadow = new TabDragShadowBuilder(getDragShadowView(view));

        // Start the drag.
        int flags = View.DRAG_FLAG_GLOBAL;
        view.startDragAndDrop(dragData, // The data to be dragged.
                tabShadow, // The drag shadow builder.
                null, // No need to use local data.
                flags);
    }

    private static class TabDragShadowBuilder extends View.DragShadowBuilder {
        public TabDragShadowBuilder(View view) {
            // Store the View parameter.
            super(view);
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

            // Set the touch point's position to be in the middle of the drag
            // shadow.
            // TODO(b/285584145): Update to accurate x and y posiiton of user hold/touch relative
            // to the Drag Shadow/Chrome Window.
            touch.set(width / 2, 50);
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

    private String getClipDataInfo(Tab clickedTab) {
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
     */
    public boolean startTabDragAction(
            View tabsToolbarView, StripLayoutHelper sourceStripLayoutHelper, Tab tabBeingDragged) {
        if (!ChromeFeatureList.sTabDragDropAndroid.isEnabled()) return false;
        if (getDragSourceTabsToolbarHashCode() != 0) return false;

        assert (tabsToolbarView != null);
        assert (sourceStripLayoutHelper != null);

        mSourceStripLayoutHelper = sourceStripLayoutHelper;

        // Build the ClipData before initiating the drag action.
        String clipData = getClipDataInfo(tabBeingDragged);
        if (clipData != null && clipData.length() > 0) {
            mTabBeingDragged = tabBeingDragged;
            initiateTabDragAndDrop(tabsToolbarView, clipData);

            // Save this View handle to identify the source view.
            mDragSourceTabsToolbarHashCode = System.identityHashCode(tabsToolbarView);
            return true;
        }
        return false;
    }

    private class DropContentReceiver implements OnReceiveContentListener {
        @Override
        public ContentInfoCompat onReceiveContent(View view, ContentInfoCompat payload) {
            // Not implemented.
            return payload;
        }
    }

    /* Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param tabsToolbarView @{link View} used to setup the drag and drop @{link
     *         View.OnDragListener}.
     * @param multiInstanceManager @{link MultiInstanceManager} to perform move action when drop
     *         completes.
     */
    public void prepareForDragDrop(
            View tabsToolbarView, MultiInstanceManager multiInstanceManager) {
        if (!ChromeFeatureList.sTabDragDropAndroid.isEnabled()) return;

        assert (tabsToolbarView != null);
        assert (multiInstanceManager != null);

        // Setup the environment.
        mPxToDp = 1.f / tabsToolbarView.getContext().getResources().getDisplayMetrics().density;
        mMultiInstanceManager = multiInstanceManager;

        // Setup a drop target and register the callback where the drag events
        // will be received.
        mDropContentReceiver = new DropContentReceiver();
        ViewCompat.setOnReceiveContentListener(
                tabsToolbarView, SUPPORTED_MIMETYPES, mDropContentReceiver);
        mOnDragListenerImpl = new OnDragListenerImpl();
        tabsToolbarView.setOnDragListener(mOnDragListenerImpl);
    }

    @VisibleForTesting
    float getPxToDp() {
        return mPxToDp;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    DropContentReceiver getDropContentReceiver() {
        return mDropContentReceiver;
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
        mDropContentReceiver = null;
        mOnDragListenerImpl = null;
        mPxToDp = 0.0f;
    }
}
