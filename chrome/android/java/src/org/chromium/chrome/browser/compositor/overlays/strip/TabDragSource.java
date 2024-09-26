// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
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
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.dragdrop.ChromeDragDropUtils;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropTabResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.widget.Toast;

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
    private Supplier<Boolean> mStripLayoutVisibilitySupplier;
    private Supplier<TabContentManager> mTabContentManagerSupplier;
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private BrowserControlsStateProvider mBrowserControlStateProvider;
    private float mPxToDp;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    @Nullable private TabModelSelector mTabModelSelector;

    /** Drag shadow properties * */
    @Nullable private StripTabDragShadowView mShadowView;

    @Nullable private Drawable mAppIcon;

    /** Drag Event Listener trackers * */
    private static TrackerToken sDragTrackerToken;

    // Drag start screen position.
    private PointF mStartScreenPos;
    // Last drag positions relative to the source view. Set when drag starts or is moved within
    // view.
    private float mLastXDp;
    private int mLastAction;
    private boolean mHoveringInStrip;
    // Local state used by Drag Drop metrics. Not-null when a tab dragging is in progress.
    private @Nullable DragLocalUmaState mUmaState;

    /**
     * Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param context Context to get resources.
     * @param stripLayoutHelperSupplier Supplier for StripLayoutHelper to perform strip actions.
     * @param multiInstanceManager MultiInstanceManager to perform move action when drop completes.
     * @param dragAndDropDelegate DragAndDropDelegate to initiate tab drag and drop.
     * @param browserControlStateProvider BrowserControlsStateProvider to compute drag-shadow
     *     dimens.
     * @param windowAndroid WindowAndroid to access activity.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     */
    public TabDragSource(
            @NonNull Context context,
            @NonNull Supplier<StripLayoutHelper> stripLayoutHelperSupplier,
            @NonNull Supplier<Boolean> stripLayoutVisibilitySupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull Supplier<LayerTitleCache> layerTitleCacheSupplier,
            @NonNull MultiInstanceManager multiInstanceManager,
            @NonNull DragAndDropDelegate dragAndDropDelegate,
            @NonNull BrowserControlsStateProvider browserControlStateProvider,
            @NonNull WindowAndroid windowAndroid,
            @NonNull ObservableSupplier<Integer> tabStripHeightSupplier) {
        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mStripLayoutHelperSupplier = stripLayoutHelperSupplier;
        mStripLayoutVisibilitySupplier = stripLayoutVisibilitySupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mMultiInstanceManager = multiInstanceManager;
        mDragAndDropDelegate = dragAndDropDelegate;
        mBrowserControlStateProvider = browserControlStateProvider;
        mWindowAndroid = windowAndroid;
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            mAppIcon = context.getPackageManager().getApplicationIcon(context.getApplicationInfo());
        }
    }

    /**
     * Starts the tab drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabBeingDragged Tab is the selected tab being dragged.
     * @param startPoint Position of the drag start point in view coordinates.
     * @param tabPositionX Horizontal position of the dragged tab in view coordinates. Used to
     *     calculate the relative position of the touch point in the tab strip.
     * @param tabWidthDp Width of the source strip tab container in dp.
     * @return true if the drag action was initiated successfully.
     */
    public boolean startTabDragAction(
            @NonNull View dragSourceView,
            @NonNull Tab tabBeingDragged,
            @NonNull PointF startPoint,
            float tabPositionX,
            float tabWidthDp) {
        // Return false when another drag in progress.
        if (DragDropGlobalState.hasValue()) {
            return false;
        }

        // Block drag for last tab in single-window mode if feature is not supported.
        if (!MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity())
                && !shouldAllowTabDragToCreateInstance()) {
            return false;
        }

        // Block drag for last tab when homepage enabled and is set to a custom url.
        if (MultiWindowUtils.getInstance().hasAtMostOneTabWithHomepageEnabled(mTabModelSelector)) {
            return false;
        }

        if (sDragTrackerToken != null) {
            Log.w(TAG, "Attempting to start drag before clearing state from prior drag");
        }

        // Allow drag to create new instance based on feature checks / current instance count.
        boolean allowDragToCreateInstance =
                shouldAllowTabDragToCreateInstance()
                        && (TabUiFeatureUtilities.doesOEMSupportDragToCreateInstance()
                                || MultiWindowUtils.getInstanceCount()
                                        < MultiWindowUtils.getMaxInstances());

        TabGroupModelFilter tabGroupModelFilter =
                (TabGroupModelFilter)
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        boolean isTabInGroup = tabGroupModelFilter.isTabInTabGroup(tabBeingDragged);

        // Build shared state with all info.
        ChromeDropDataAndroid dropData =
                new ChromeDropDataAndroid.Builder()
                        .withTab(tabBeingDragged)
                        .withTabInGroup(isTabInGroup)
                        .withAllowDragToCreateInstance(allowDragToCreateInstance)
                        .build();
        updateShadowView(tabBeingDragged, dragSourceView, (int) (tabWidthDp / mPxToDp));
        DragShadowBuilder builder =
                createDragShadowBuilder(dragSourceView, startPoint, tabPositionX);
        sDragTrackerToken =
                DragDropGlobalState.store(
                        mMultiInstanceManager.getCurrentInstanceId(), dropData, builder);
        boolean res = mDragAndDropDelegate.startDragAndDrop(dragSourceView, builder, dropData);
        if (!res) {
            DragDropGlobalState.clear(sDragTrackerToken);
            sDragTrackerToken = null;
        }
        return res;
    }

    @VisibleForTesting
    void updateShadowView(
            @NonNull Tab tabBeingDragged, @NonNull View dragSourceView, int tabWidthPx) {
        // Shadow view is unused for drag as window.
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) return;
        if (mShadowView == null) {
            View rootView =
                    View.inflate(
                            dragSourceView.getContext(),
                            R.layout.strip_tab_drag_shadow_view,
                            (ViewGroup) dragSourceView.getRootView());
            mShadowView = rootView.findViewById(R.id.strip_tab_drag_shadow_view);

            mShadowView.initialize(
                    mBrowserControlStateProvider,
                    mTabContentManagerSupplier,
                    mLayerTitleCacheSupplier,
                    () -> {
                        TabDragShadowBuilder builder =
                                (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
                        // We register callbacks (e.g. to update the thumbnail) that may attempt to
                        // update the shadow after the drop has already ended. No-op in that case.
                        if (builder != null) {
                            showDragShadow(builder.mShowDragShadow);
                        }
                    });
        }
        mShadowView.prepareForDrag(tabBeingDragged, tabWidthPx);
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean res = false;
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                res =
                        onDragStart(
                                dragEvent.getX(), dragEvent.getY(), dragEvent.getClipDescription());
                if (res) mUmaState = new DragLocalUmaState();
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                res =
                        onDragEnd(
                                view,
                                dragEvent.getX(),
                                dragEvent.getY(),
                                dragEvent.getResult(),
                                mLastAction == DragEvent.ACTION_DRAG_EXITED);
                mUmaState = null;
                break;
            case DragEvent.ACTION_DRAG_ENTERED:
                // We'll trigger #onDragEnter when handling the following ACTION_DRAG_LOCATION so we
                // have position data available (and can check if we've entered the tab strip).
                res = false;
                break;
            case DragEvent.ACTION_DRAG_EXITED:
                if (mHoveringInStrip) res = onDragExit();
                break;
            case DragEvent.ACTION_DRAG_LOCATION:
                boolean isCurrYInTabStrip = didOccurInTabStrip(dragEvent.getY());
                if (isCurrYInTabStrip) {
                    if (!mHoveringInStrip) {
                        // dragged onto strip from outside controls OR from toolbar.
                        res = onDragEnter(dragEvent.getX());
                    } else {
                        // drag moved within strip.
                        res = onDragLocation(dragEvent.getX(), dragEvent.getY());
                    }
                    mLastXDp = dragEvent.getX() * mPxToDp;
                } else if (mHoveringInStrip) {
                    // drag moved from within to outside strip.
                    res = onDragExit();
                }
                break;
            case DragEvent.ACTION_DROP:
                if (didOccurInTabStrip(dragEvent.getY())) {
                    res = onDrop(dragEvent);
                } else {
                    DragDropMetricUtils.recordTabDragDropResult(DragDropTabResult.IGNORED_TOOLBAR);
                    res = false;
                }
                break;
        }
        mLastAction = dragEvent.getAction();
        return res;
    }

    /** Sets @{@link TabModelSelector} to retrieve model info. */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /** Whether a tab drag and drop has started. */
    public boolean isTabDraggingInProgress() {
        return sDragTrackerToken != null;
    }

    private boolean didOccurInTabStrip(float yPx) {
        return yPx <= mTabStripHeightSupplier.get();
    }

    private boolean onDragStart(float xPx, float yPx, ClipDescription clipDescription) {
        if (clipDescription == null
                || clipDescription.filterMimeTypes(MimeTypeUtils.CHROME_MIMETYPE_TAB) == null) {
            return false;
        }

        // Return true only when the tab strip is visible.
        // Otherwise, return false to not receive further events until dragEnd.
        if (!isDragSource()) {
            return Boolean.TRUE.equals(mStripLayoutVisibilitySupplier.get());
        }

        mStartScreenPos = new PointF(xPx, yPx);
        mLastXDp = xPx * mPxToDp;
        return true;
    }

    private boolean onDragEnter(float xPx) {
        mHoveringInStrip = true;
        boolean isDragSource = isDragSource();
        if (!isDragSource && mUmaState.mTabEnteringDestStripSystemElapsedTime < 0) {
            mUmaState.mTabEnteringDestStripSystemElapsedTime = SystemClock.elapsedRealtime();
        }
        if (isDragSource || TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            showDragShadow(false);
        }
        mStripLayoutHelperSupplier
                .get()
                .prepareForTabDrop(
                        LayoutManagerImpl.time(),
                        xPx * mPxToDp,
                        mLastXDp,
                        isDragSource,
                        isDraggedTabIncognito());
        return true;
    }

    private boolean onDragLocation(float xPx, float yPx) {
        float xDp = xPx * mPxToDp;
        float yDp = yPx * mPxToDp;
        mStripLayoutHelperSupplier
                .get()
                .dragForTabDrop(
                        LayoutManagerImpl.time(),
                        xDp,
                        yDp,
                        xDp - mLastXDp,
                        isDraggedTabIncognito());
        return true;
    }

    private boolean onDrop(DragEvent dropEvent) {
        StripLayoutHelper helper = mStripLayoutHelperSupplier.get();
        int destinationTabId = helper.getTabDropId();
        helper.onUpOrCancel(LayoutManagerImpl.time());

        if (isDragSource()) {
            DragDropMetricUtils.recordTabReorderStripWithDragDrop(mUmaState.mDragEverLeftStrip);
            return true;
        }

        if (dropEvent.getClipDescription() == null
                || !dropEvent.getClipDescription().hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB)) {
            return false;
        }

        Tab tabBeingDragged = getTabFromGlobalState(dropEvent);
        if (tabBeingDragged == null) {
            return false;
        }
        boolean tabDraggedBelongToCurrentModel = doesBelongToCurrentModel(tabBeingDragged);

        // Record user action if a grouped tab is going to be re-parented.
        recordTabRemovedFromGroupUserAction();

        // Move tab to another window.
        if (!tabDraggedBelongToCurrentModel) {
            mMultiInstanceManager.moveTabToWindow(
                    getActivity(),
                    tabBeingDragged,
                    mTabModelSelector.getModel(tabBeingDragged.isIncognito()).getCount());
            showDroppedDifferentModelToast(mWindowAndroid.getContext().get());
        } else {
            int tabIndex = helper.getTabIndexForTabDrop(dropEvent.getX() * mPxToDp);
            mMultiInstanceManager.moveTabToWindow(getActivity(), tabBeingDragged, tabIndex);
            helper.mergeToGroupForTabDropIfNeeded(
                    destinationTabId, tabBeingDragged.getId(), tabIndex);
        }
        DragDropMetricUtils.recordTabDragDropType(DragDropType.TAB_STRIP_TO_TAB_STRIP);
        mUmaState.mTabLeavingDestStripSystemElapsedTime = SystemClock.elapsedRealtime();
        return true;
    }

    private boolean onDragEnd(
            View view, float xPx, float yPx, boolean dropHandled, boolean didExitToolbar) {
        mHoveringInStrip = false;

        // No-op for destination strip. Note: If we add updates for target strip, also check for
        // !TabUiFeatureUtilities.DISABLE_STRIP_TO_STRIP_DD.getValue()
        if (!isDragSource()) {
            if (mUmaState.mTabEnteringDestStripSystemElapsedTime > 0
                    && mUmaState.mTabLeavingDestStripSystemElapsedTime > 0) {
                long duration =
                        mUmaState.mTabLeavingDestStripSystemElapsedTime
                                - mUmaState.mTabEnteringDestStripSystemElapsedTime;
                assert duration >= 0
                        : "Duration when the drag is within the destination strip is invalid";
                DragDropMetricUtils.recordTabDurationWithinDestStrip(duration);
            }
            return false;
        }

        // If tab was dragged and dropped out of source toolbar but the drop was not handled,
        // move to a new window.
        Tab tabBeingDragged = getTabFromGlobalState(null);
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()
                && didExitToolbar
                && !dropHandled
                && tabBeingDragged != null) {
            // Following call is device specific and is intended for specific platform
            // SysUI.
            sendPositionInfoToSysUI(view, mStartScreenPos.x, mStartScreenPos.y, xPx, yPx);

            // Record user action if a grouped tab is moved to a new window.
            recordTabRemovedFromGroupUserAction();

            // Hence move the tab to a new Chrome window.
            mMultiInstanceManager.moveTabToNewWindow(tabBeingDragged);
        }

        // Get the drag source Chrome instance id before it is cleared as it may be closed.
        int sourceInstanceId =
                DragDropGlobalState.getState(sDragTrackerToken).getDragSourceInstance();

        mStripLayoutHelperSupplier.get().clearTabDragState();
        if (mShadowView != null) {
            mShadowView.clear();
        }
        if (sDragTrackerToken != null) {
            DragDropGlobalState.clear(sDragTrackerToken);
            sDragTrackerToken = null;
        }

        // Close the source instance window if it has no tabs.
        boolean didCloseWindow = mMultiInstanceManager.closeChromeWindowIfEmpty(sourceInstanceId);

        // Only record for source strip to avoid duplicate.
        if (dropHandled) {
            DragDropMetricUtils.recordTabDragDropResult(DragDropTabResult.SUCCESS);
            DragDropMetricUtils.recordTabDragDropClosedWindow(didCloseWindow);
        } else if (MultiWindowUtils.getInstanceCount() == MultiWindowUtils.getMaxInstances()) {
            Toast.makeText(
                            mWindowAndroid.getContext().get(),
                            R.string.max_number_of_windows,
                            Toast.LENGTH_LONG)
                    .show();
            ChromeDragDropUtils.recordTabDragToCreateInstanceFailureCount();
            DragDropMetricUtils.recordTabDragDropResult(DragDropTabResult.IGNORED_MAX_INSTANCES);
        }

        return true;
    }

    private void recordTabRemovedFromGroupUserAction() {
        DragDropGlobalState globalState = DragDropGlobalState.getState(sDragTrackerToken);
        if (globalState.getData() instanceof ChromeDropDataAndroid
                && ((ChromeDropDataAndroid) globalState.getData()).isTabInGroup) {
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
        }
    }

    private boolean onDragExit() {
        mHoveringInStrip = false;
        mUmaState.mDragEverLeftStrip = true;
        if (!isDragSource()) {
            mUmaState.mTabLeavingDestStripSystemElapsedTime = SystemClock.elapsedRealtime();
        }
        if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            showDragShadow(true);
        } else if (isDragSource()) {
            TabDragShadowBuilder builder =
                    (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
            if (builder != null) {
                builder.mShowDragShadow = true;
                mShadowView.expand();
            }
        }
        mStripLayoutHelperSupplier
                .get()
                .clearForTabDrop(LayoutManagerImpl.time(), isDragSource(), isDraggedTabIncognito());
        return true;
    }

    private static void showDragShadow(boolean show) {
        if (!DragDropGlobalState.hasValue()) {
            Log.w(TAG, "Global state is null when try to update drag shadow.");
            return;
        }

        TabDragShadowBuilder builder =
                (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
        if (builder == null) return;
        builder.update(show);
    }

    private Tab getTabFromGlobalState(@Nullable DragEvent dragEvent) {
        DragDropGlobalState globalState =
                dragEvent != null
                        ? DragDropGlobalState.getState(dragEvent)
                        : DragDropGlobalState.getState(sDragTrackerToken);
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        assert globalState.getData() instanceof ChromeDropDataAndroid
                : "Attempting to access dragged tab with wrong data type";
        return ((ChromeDropDataAndroid) globalState.getData()).tab;
    }

    private boolean isDragSource() {
        DragDropGlobalState globalState = DragDropGlobalState.getState(sDragTrackerToken);
        // May attempt to check source on drag end.
        if (globalState == null) return false;
        return globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId());
    }

    private boolean isDraggedTabIncognito() {
        return getTabFromGlobalState(null).isIncognito();
    }

    /**
     * Shows a toast indicating that a tab is dropped into strip in a different model.
     *
     * @param context The context where the toast will be shown.
     */
    private void showDroppedDifferentModelToast(Context context) {
        Toast.makeText(context, R.string.tab_dropped_different_model, Toast.LENGTH_LONG).show();
    }

    private boolean doesBelongToCurrentModel(Tab tabBeingDragged) {
        return mTabModelSelector.getCurrentModel().isIncognito() == tabBeingDragged.isIncognito();
    }

    private Activity getActivity() {
        assert mWindowAndroid.getActivity().get() != null;
        return mWindowAndroid.getActivity().get();
    }

    private View getDecorView() {
        return getActivity().getWindow().getDecorView();
    }

    @VisibleForTesting
    static class TabDragShadowBuilder extends View.DragShadowBuilder {
        // Touch offset for drag shadow view.
        private PointF mDragShadowOffset;
        // Source initiating drag - to call updateDragShadow().
        private View mDragSourceView;
        // Content to add to shadowView.
        @Nullable private Drawable mViewContent;
        // Whether drag shadow should be shown.
        private boolean mShowDragShadow;

        public TabDragShadowBuilder(
                View dragSourceView,
                View shadowView,
                PointF dragShadowOffset,
                Drawable viewContent) {
            // Store the View parameter.
            super(shadowView);
            mDragShadowOffset = dragShadowOffset;
            mDragSourceView = dragSourceView;
            mViewContent = viewContent;
        }

        public void update(boolean show) {
            mShowDragShadow = show;
            mDragSourceView.updateDragShadow(this);
        }

        @Override
        public void onDrawShadow(@NonNull Canvas canvas) {
            View shadowView = getView();
            if (mShowDragShadow) {
                if (TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
                    assert mViewContent != null;
                    ((ImageView) shadowView).setImageDrawable(mViewContent);
                    shadowView.setBackgroundDrawable(new ColorDrawable(Color.LTGRAY));
                    // Pad content to the center of the drag shadow.
                    int paddingHorizontal =
                            (shadowView.getWidth() - mViewContent.getIntrinsicWidth()) / 2;
                    int paddingVertical =
                            (shadowView.getHeight() - mViewContent.getIntrinsicHeight()) / 2;
                    shadowView.setPadding(
                            paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical);
                    shadowView.layout(0, 0, shadowView.getWidth(), shadowView.getHeight());
                }
                shadowView.draw(canvas);
            } else {
                // When drag shadow should hide, replace with empty ImageView.
                ImageView imageView = new ImageView(shadowView.getContext());
                imageView.layout(0, 0, shadowView.getWidth(), shadowView.getHeight());
                imageView.draw(canvas);
            }
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
            touch.set(Math.round(mDragShadowOffset.x), Math.round(mDragShadowOffset.y));
            Log.d(TAG, "DnD onProvideShadowMetrics: " + mDragShadowOffset);
        }

        boolean getShadowShownForTesting() {
            return mShowDragShadow;
        }
    }

    DragShadowBuilder createDragShadowBuilder(
            View dragSourceView, PointF startPoint, float tabPositionX) {
        PointF dragShadowOffset;
        if (!TabUiFeatureUtilities.isTabDragAsWindowEnabled()) {
            // Set the touch point of the drag shadow:
            // Horizontally matching user's touch point within the tab title;
            // Vertically centered in the tab title.
            Resources resources = dragSourceView.getContext().getResources();
            float dragShadowOffsetY =
                    resources.getDimension(R.dimen.tab_grid_card_header_height) / 2
                            + resources.getDimension(R.dimen.tab_grid_card_margin);
            dragShadowOffset =
                    new PointF((startPoint.x - tabPositionX) / mPxToDp, dragShadowOffsetY);
            return new TabDragShadowBuilder(dragSourceView, mShadowView, dragShadowOffset, null);
        }
        ImageView imageView = new ImageView(dragSourceView.getContext());
        View decorView = getDecorView();
        imageView.layout(0, 0, decorView.getWidth(), decorView.getHeight());
        // Set the touch point of the drag shadow to be user's hold/touch point within Chrome
        // Window.
        dragShadowOffset = getPositionOnScreen(dragSourceView, startPoint);
        return new TabDragShadowBuilder(dragSourceView, imageView, dragShadowOffset, mAppIcon);
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

    private boolean shouldAllowTabDragToCreateInstance() {
        return hasMultipleTabs(mTabModelSelector)
                && TabUiFeatureUtilities.isTabDragToCreateInstanceSupported();
    }

    private boolean hasMultipleTabs(TabModelSelector tabModelSelector) {
        return tabModelSelector != null && tabModelSelector.getTotalTabCount() > 1;
    }

    View getShadowViewForTesting() {
        return mShadowView;
    }

    static class DragLocalUmaState {
        // Whether the tab drag has ever left the source strip.
        boolean mDragEverLeftStrip;
        // The SystemElapsedTime when the tab dragged first enters the destination strip.
        long mTabEnteringDestStripSystemElapsedTime;
        // The SystemElapsedTime when the tab dragged exits or drops into the destination strip.
        long mTabLeavingDestStripSystemElapsedTime;

        DragLocalUmaState() {
            mDragEverLeftStrip = false;
            mTabEnteringDestStripSystemElapsedTime = -1;
            mTabLeavingDestStripSystemElapsedTime = -1;
        }
    }
}
