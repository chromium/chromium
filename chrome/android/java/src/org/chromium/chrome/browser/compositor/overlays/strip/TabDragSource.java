// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DropDataAndroid;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * StripLayoutHelper}.
 */
public class TabDragSource implements View.OnDragListener {
    private static final String TAG = "TabDragSource";
    private final WindowAndroid mWindowAndroid;
    private MultiInstanceManager mMultiInstanceManager;
    private DragAndDropDelegate mDragAndDropDelegate;
    private Supplier<StripLayoutHelper> mStripLayoutHelperSupplier;
    private Supplier<TabContentManager> mTabContentManagerSupplier;
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private View mDragSourceView;
    private StripTabDragShadowView mShadowView;
    private PointF mDragShadowDefaultOffset = new PointF(0, 0);
    private float mPxToDp;
    private final float mTabStripHeightPx;

    /** Drag Event Listener trackers * */
    // Drag start screen position.
    private PointF mStartScreenPos;

    // Last drag positions relative to the source view. Set when drag starts or is moved within
    // view.
    private float mLastXDp;
    private float mLastYDp;
    private int mLastAction;
    @Nullable private TabModelSelector mTabModelSelector;

    /**
     * Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param context @{@link Context} to get resources.
     * @param stripLayoutHelperSupplier Supplier for @{@link StripLayoutHelper} to perform strip
     *     actions.
     * @param multiInstanceManager @{link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate @{@link DragAndDropDelegate} to initiate tab drag and drop.
     * @param browserControlStateProvider @{@link BrowserControlsStateProvider} to compute
     *     drag-shadow dimens.
     * @param windowAndroid @{@link WindowAndroid} to access activity.
     */
    public TabDragSource(
            @NonNull Context context,
            @NonNull Supplier<StripLayoutHelper> stripLayoutHelperSupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull Supplier<LayerTitleCache> layerTitleCacheSupplier,
            @NonNull MultiInstanceManager multiInstanceManager,
            @NonNull DragAndDropDelegate dragAndDropDelegate,
            @NonNull BrowserControlsStateProvider browserControlStateProvider,
            @NonNull WindowAndroid windowAndroid) {
        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        // TODO(crbug.com/1498252): Use Toolbar#getTabStripHeight() instead.
        mTabStripHeightPx = context.getResources().getDimension(R.dimen.tab_strip_height);
        mStripLayoutHelperSupplier = stripLayoutHelperSupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mMultiInstanceManager = multiInstanceManager;
        mDragAndDropDelegate = dragAndDropDelegate;
        mBrowserControlStateProvider = browserControlStateProvider;
        mWindowAndroid = windowAndroid;
    }

    /**
     * Starts the tab drag action by initiating the process by calling @{link
     * View.startDragAndDrop}.
     *
     * @param toolbarContainerView @{link View} used to create the drag shadow.
     * @param tabBeingDragged @{link Tab} is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     */
    public boolean startTabDragAction(
            @NonNull View toolbarContainerView,
            @NonNull Tab tabBeingDragged,
            @NonNull PointF startPoint) {
        if (!TabUiFeatureUtilities.isTabDragEnabled()
                || DragDropGlobalState.getInstance().dragSourceInstanceId
                        != MultiWindowUtils.INVALID_INSTANCE_ID) {
            return false;
        }
        if (!MultiWindowUtils.getInstance()
                .isMoveToOtherWindowSupported(getActivity(), mTabModelSelector)) {
            return false;
        }

        setGlobalState(tabBeingDragged);

        if (mShadowView == null) {
            View rootView =
                    View.inflate(
                            toolbarContainerView.getContext(),
                            R.layout.strip_tab_drag_shadow_view,
                            (ViewGroup) toolbarContainerView.getRootView());
            mShadowView = rootView.findViewById(R.id.strip_tab_drag_shadow_view);

            mShadowView.initialize(
                    mBrowserControlStateProvider,
                    mTabContentManagerSupplier,
                    mLayerTitleCacheSupplier,
                    () -> {
                        if (DragDropGlobalState.getInstance().dragShadowShowing) {
                            showDragShadow(true);
                        }
                    });
        }
        mShadowView.setTab(tabBeingDragged);

        mDragSourceView = toolbarContainerView;
        mDragShadowDefaultOffset =
                TabUiFeatureUtilities.isTabDragAsWindowEnabled()
                        ? getPositionOnScreen(toolbarContainerView, startPoint)
                        : new PointF(0f, 0f);

        DropDataAndroid dropData =
                new ChromeDropDataAndroid.Builder().withTabId(tabBeingDragged.getId()).build();
        DragShadowBuilder builder =
                createTabDragShadowBuilder(toolbarContainerView.getContext(), false);
        return mDragAndDropDelegate.startDragAndDrop(toolbarContainerView, builder, dropData);
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean res = false;
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                res =
                        onDragStart(
                                dragEvent.getX(), dragEvent.getY(), dragEvent.getClipDescription());
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                res =
                        onDragEnd(
                                view,
                                dragEvent.getX(),
                                dragEvent.getY(),
                                dragEvent.getResult(),
                                mLastAction == DragEvent.ACTION_DRAG_EXITED);
                break;
            case DragEvent.ACTION_DRAG_ENTERED:
                res = didOccurInTabStrip(dragEvent.getY()) ? onDragEnter() : false;
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                res = onDragExit();
                break;
            case DragEvent.ACTION_DRAG_LOCATION:
                boolean isLastYInTabStrip = didOccurInTabStrip(mLastYDp / mPxToDp);
                if (mLastAction == DragEvent.ACTION_DRAG_ENTERED
                        || (isLastYInTabStrip && didOccurInTabStrip(dragEvent.getY()))) {
                    // First move after drag enter OR drag moved within strip
                    res = onDragLocation(dragEvent.getX(), dragEvent.getY());
                } else if (isLastYInTabStrip) {
                    // drag moved from within to outside strip.
                    res = onDragExit();
                } else if (didOccurInTabStrip(dragEvent.getY())) {
                    // drag moved from outside to within strip.
                    res = onDragEnter();
                }
                mLastXDp = dragEvent.getX() * mPxToDp;
                mLastYDp = dragEvent.getY() * mPxToDp;
                break;
            case DragEvent.ACTION_DROP:
                res =
                        didOccurInTabStrip(dragEvent.getY())
                                ? onDrop(dragEvent.getX(), dragEvent.getClipData())
                                : false;
                break;
        }
        mLastAction = dragEvent.getAction();
        return res;
    }

    /** Sets @{@link TabModelSelector} to retrieve model info. */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    private boolean didOccurInTabStrip(float yPx) {
        return yPx <= mTabStripHeightPx;
    }

    private boolean onDragStart(float xPx, float yPx, ClipDescription clipDescription) {
        if (clipDescription.filterMimeTypes(ChromeDragAndDropBrowserDelegate.CHROME_MIMETYPE_TAB)
                == null) {
            return false;
        }
        if (!isDragSource()) return true;
        mStartScreenPos = new PointF(xPx, yPx);
        mLastXDp = xPx * mPxToDp;
        mLastYDp = yPx * mPxToDp;
        return true;
    }

    private boolean onDragEnter() {
        if (!isDragSource()) return false;
        mStripLayoutHelperSupplier
                .get()
                .dragActiveClickedTabOntoStrip(LayoutManagerImpl.time(), mLastXDp);
        showDragShadow(false);
        return true;
    }

    private boolean onDragLocation(float xPx, float yPx) {
        if (!isDragSource()) return false;
        float xDp = xPx * mPxToDp;
        float yDp = yPx * mPxToDp;
        mStripLayoutHelperSupplier.get().drag(LayoutManagerImpl.time(), xDp, yDp, xDp - mLastXDp);
        return true;
    }

    private boolean onDrop(float xPx, ClipData clipData) {
        if (isDragSource()) {
            mStripLayoutHelperSupplier.get().onUpOrCancel(LayoutManagerImpl.time());
            return true;
        }

        // If the event is received by a non source chrome window then accept the drop
        // in the destination chrome window.
        for (int i = 0; i < clipData.getItemCount(); i++) {
            int sourceTabId = getTabIdFromClipData(clipData.getItemAt(i));
            // Ignore the drop if the dropped tab id does not match the id of tab being
            // dragged. Return the original payload drop for next in line to receive the
            // drop to handle.
            Tab tabBeingDragged = DragDropGlobalState.getInstance().tabBeingDragged;
            if (tabBeingDragged == null
                    || sourceTabId != tabBeingDragged.getId()
                    || mTabModelSelector == null) {
                Log.w(TAG, "DnD: Received an invalid tab drop.");
                return false;
            }
            int tabPositionIndex =
                    getTabPositionIndex(xPx * mPxToDp, tabBeingDragged.isIncognito());
            mMultiInstanceManager.moveTabToWindow(getActivity(), tabBeingDragged, tabPositionIndex);
        }
        return true;
    }

    private boolean onDragEnd(
            View view, float xPx, float yPx, boolean dropHandled, boolean didExitToolbar) {
        if (!isDragSource()) return false;
        // If tab was dragged and dropped out of source toolbar but the drop was not handled, move
        // to a new window.
        Tab tabBeingDragged = DragDropGlobalState.getInstance().tabBeingDragged;
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()
                && didExitToolbar
                && !dropHandled
                && DragDropGlobalState.getInstance().tabBeingDragged != null) {
            // Following call is device specific and is intended for specific platform
            // SysUI.
            sendPositionInfoToSysUI(view, mStartScreenPos.x, mStartScreenPos.y, xPx, yPx);

            // Hence move the tab to a new Chrome window.
            mMultiInstanceManager.moveTabToNewWindow(tabBeingDragged);
        }

        // Notify DragNDrop is completed.
        DragDropGlobalState.getInstance().reset();
        // TODO (crbug.com/1497784): Remove this method.
        mStripLayoutHelperSupplier.get().clearActiveClickedTab();
        mShadowView.clear();
        return true;
    }

    private boolean onDragExit() {
        if (!isDragSource()) return false;
        // Show drag shadow when drag exits strip.
        // TODO (crbug.com/1497784): Call this once on first drag exit. Reset on drag end.
        showDragShadow(true);
        mStripLayoutHelperSupplier.get().dragActiveClickedTabOutOfStrip(LayoutManagerImpl.time());
        return true;
    }

    private void showDragShadow(boolean show) {
        assert mDragSourceView != null;
        DragDropGlobalState.getInstance().dragShadowShowing = show;
        DragShadowBuilder builder = createTabDragShadowBuilder(mDragSourceView.getContext(), show);
        mDragSourceView.updateDragShadow(builder);
    }

    private boolean isDragSource() {
        return DragDropGlobalState.getInstance().dragSourceInstanceId
                == mMultiInstanceManager.getCurrentInstanceId();
    }

    private int getTabIdFromClipData(ClipData.Item item) {
        // TODO(b/285585036): Expand the ClipData definition to support dropping of the Tab info to
        // be used by SysUI that can parse this format.
        String[] itemTexts = item.getText().toString().split(";");
        String numberText = itemTexts[0].replaceAll("[^0-9]", "");
        return numberText.isEmpty() ? Tab.INVALID_TAB_ID : Integer.parseInt(numberText);
    }

    private int getTabPositionIndex(float dropXDp, boolean isDraggedTabIncognito) {
        StripLayoutHelper activeStripHelper = mStripLayoutHelperSupplier.get();
        // If dragged tab and drop target strip don't belong to same model,
        // drop tab at corresponding model at end of strip.
        if (mTabModelSelector.getCurrentModel().isIncognito() != isDraggedTabIncognito) {
            TabModel model = mTabModelSelector.getModel(isDraggedTabIncognito);
            return model.getCount();
        }
        // Based on the location of the drop determine the position index where the tab will be
        // placed.
        StripLayoutTab droppedOn = activeStripHelper.getTabAtPosition(dropXDp);
        int tabPositionIndex = mTabModelSelector.getCurrentModel().getCount();
        // If not dropped on any existing tabs then simply add it at the end.
        if (droppedOn != null) {
            tabPositionIndex = activeStripHelper.findIndexForTab(droppedOn.getId());
            // Check if the tab being moved needs to be added before or after the tab it was
            // dropped on based on the layout direction of tabs.
            float droppedTabCenterX = droppedOn.getDrawX() + droppedOn.getWidth() / 2.f;
            if (LocalizationUtils.isLayoutRtl()) {
                if (dropXDp <= droppedTabCenterX) {
                    tabPositionIndex++;
                }
            } else {
                if (dropXDp > droppedTabCenterX) {
                    tabPositionIndex++;
                }
            }
        }
        return tabPositionIndex;
    }

    private Activity getActivity() {
        assert mWindowAndroid.getActivity().get() != null;
        return mWindowAndroid.getActivity().get();
    }

    private View getDecorView() {
        return getActivity().getWindow().getDecorView();
    }

    @VisibleForTesting
    void setGlobalState(Tab tabBeingDragged) {
        // TODO (crbug.com/1497784): Move to startDragAndDrop call.
        DragDropGlobalState.getInstance().tabBeingDragged = tabBeingDragged;
        DragDropGlobalState.getInstance().dragSourceInstanceId =
                mMultiInstanceManager.getCurrentInstanceId();
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

    @NonNull
    @VisibleForTesting
    DragShadowBuilder createTabDragShadowBuilder(Context context, boolean show) {
        int shadowWidthPx;
        int shadowHeightPx;
        ImageView imageView = new ImageView(context);
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            // View is empty and nothing is shown for now.
            // Get Chrome window dimensions and set the view to that size.
            View decorView = getDecorView();
            shadowWidthPx = decorView.getWidth();
            shadowHeightPx = decorView.getHeight();
            if (show) {
                addAppIconToShadow(imageView, context, shadowWidthPx, shadowHeightPx);
            }
        } else {
            if (show) {
                return new TabDragShadowBuilder(mShadowView, mDragShadowDefaultOffset);
            }
            shadowWidthPx = mShadowView.getWidth();
            shadowHeightPx = mShadowView.getHeight();
        }
        if (show) {
            imageView.setBackgroundDrawable(new ColorDrawable(Color.LTGRAY));
        }
        imageView.layout(0, 0, shadowWidthPx, shadowHeightPx);
        return new TabDragShadowBuilder(imageView, mDragShadowDefaultOffset);
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

    private void sendPositionInfoToSysUI(
            View view,
            float startXInView,
            float startYInView,
            float endXInScreen,
            float endYInScreen) {
        // The start position is in the view coordinate system and related to the top left position
        // of the toolbar container view. Convert it to the screen coordinate system for comparison
        // with the drop position which is in screen coordinates.
        int[] topLeftLocation = new int[2];
        // TODO (crbug.com/1497784): Use mDragSourceView instead.
        view.getLocationOnScreen(topLeftLocation);
        float startXInScreen = topLeftLocation[0] + startXInView;
        float startYInScreen = topLeftLocation[1] + startYInView;

        DisplayMetrics displayMetrics = view.getContext().getResources().getDisplayMetrics();
        int windowWidthPx = displayMetrics.widthPixels;
        int windowHeightPx = displayMetrics.heightPixels;

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
        float xOffsetRelative2WindowWidth = (endXInScreen - startXInScreen) / windowWidthPx;
        float yOffsetRelative2WindowHeight = (endYInScreen - startYInScreen) / windowHeightPx;

        // Prepare the positioning intent for SysUI to place the next Chrome window.
        // The intent is ignored when not handled with no impact on existing Android platforms.
        Intent intent = new Intent();
        intent.setPackage("com.android.systemui");
        intent.setAction("com.android.systemui.CHROME_TAB_DRAG_DROP");
        int taskId = ApplicationStatus.getTaskId(getActivity());
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_TASK_ID", taskId);
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_OFFSET_X", xOffsetRelative2WindowWidth);
        intent.putExtra("CHROME_TAB_DRAG_DROP_ANCHOR_OFFSET_Y", yOffsetRelative2WindowHeight);
        mWindowAndroid.sendBroadcast(intent);
        Log.d(
                TAG,
                "DnD Position info for SysUI: tId="
                        + taskId
                        + ", xOff="
                        + xOffsetRelative2WindowWidth
                        + ", yOff="
                        + yOffsetRelative2WindowHeight);
    }

    private PointF getPositionOnScreen(View view, PointF positionInView) {
        int[] topLeftLocationOfToolbarView = new int[2];
        view.getLocationOnScreen(topLeftLocationOfToolbarView);

        int[] topLeftLocationOfDecorView = new int[2];
        getDecorView().getLocationOnScreen(topLeftLocationOfDecorView);

        float positionXOnScreen =
                (topLeftLocationOfToolbarView[0] - topLeftLocationOfDecorView[0])
                        + positionInView.x / mPxToDp;
        float positionYOnScreen =
                (topLeftLocationOfToolbarView[1] - topLeftLocationOfDecorView[1])
                        + positionInView.y / mPxToDp;
        return new PointF(positionXOnScreen, positionYOnScreen);
    }

    View getShadowViewForTesting() {
        return mShadowView;
    }
}
