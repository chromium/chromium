// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.app.Activity;
import android.content.ClipDescription;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Handler;
import android.os.Looper;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.dragdrop.ChromeDragDropUtils;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.util.XrUtils;
import org.chromium.ui.widget.Toast;

import java.util.Collections;
import java.util.List;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * StripLayoutHelper}.
 */
public class TabDragSource implements View.OnDragListener {
    private static final String TAG = "TabDragSource";

    private final WindowAndroid mWindowAndroid;
    private final MultiInstanceManager mMultiInstanceManager;
    private final DragAndDropDelegate mDragAndDropDelegate;
    private final Supplier<StripLayoutHelper> mStripLayoutHelperSupplier;
    private final Supplier<Boolean> mStripLayoutVisibilitySupplier;
    private final Supplier<TabContentManager> mTabContentManagerSupplier;
    private final Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private final BrowserControlsStateProvider mBrowserControlStateProvider;
    private final float mPxToDp;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private final DesktopWindowStateManager mDesktopWindowStateManager;

    /** Handler and runnable to post/cancel an #onDragExit when the drag starts. */
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final Runnable mOnDragExitRunnable = this::onDragExit;

    @Nullable private TabModelSelector mTabModelSelector;

    /** Drag shadow properties */
    @Nullable private StripDragShadowView mShadowView;

    private ObservableSupplier<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier;
    private MultiThumbnailCardProvider mMultiThumbnailCardProvider;

    /** Drag Event Listener trackers */
    private static TrackerToken sDragTrackerToken;

    // Last drag positions relative to the source view. Set when drag starts or is moved within
    // view.
    private float mLastXDp;
    private boolean mHoveringInStrip;

    // Tracks whether the current drag has ever left the source strip.
    private boolean mDragEverLeftStrip;

    /**
     * Prepares the toolbar view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param context Context to get resources.
     * @param stripLayoutHelperSupplier Supplier for StripLayoutHelper to perform strip actions.
     * @param stripLayoutVisibilitySupplier Supplier for if the given tab strip is visible.
     * @param tabContentManagerSupplier Supplier for the TabContentManager.
     * @param layerTitleCacheSupplier Supplier for the LayerTitleCache.
     * @param multiInstanceManager MultiInstanceManager to perform move action when drop completes.
     * @param dragAndDropDelegate DragAndDropDelegate to initiate tab drag and drop.
     * @param browserControlStateProvider BrowserControlsStateProvider to compute drag-shadow
     *     dimens.
     * @param windowAndroid WindowAndroid to access activity.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance to determine
     *     desktop windowing mode state.
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
            @NonNull ObservableSupplier<Integer> tabStripHeightSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
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
        mDesktopWindowStateManager = desktopWindowStateManager;
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
        if (isDragAlreadyInProgress()) {
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

        // Allow drag to create new instance based on feature checks / current instance count.
        boolean allowDragToCreateInstance =
                shouldAllowTabDragToCreateInstance()
                        && (TabUiFeatureUtilities.doesOemSupportDragToCreateInstance()
                                || MultiWindowUtils.getInstanceCount()
                                        < MultiWindowUtils.getMaxInstances());

        boolean isTabInGroup =
                mCurrentTabGroupModelFilterSupplier.get().isTabInTabGroup(tabBeingDragged);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());

        // Build shared state with all info.
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder()
                        .withTab(tabBeingDragged)
                        .withTabInGroup(isTabInGroup)
                        .withAllowDragToCreateInstance(allowDragToCreateInstance)
                        .withWindowId(windowId)
                        .build();

        // Initialize drag shadow.
        initShadowView(dragSourceView);
        if (mShadowView != null) {
            mShadowView.prepareForTabDrag(tabBeingDragged, (int) (tabWidthDp / mPxToDp));
        }

        return startDragAction(dropData, startPoint, tabPositionX, dragSourceView);
    }

    /**
     * Starts the group drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabGroupId The dragged group's ID.
     * @param isGroupShared Whether the dragged group is shared with other collaborators.
     * @param startPoint Position of the drag start point in view coordinates.
     * @param positionX The horizontal position of the dragged group title in view coordinates.
     * @param widthDp Width of the group title in dp.
     * @return {@code True} if the drag action was initiated successfully.
     */
    public boolean startGroupDragAction(
            @NonNull View dragSourceView,
            @NonNull Token tabGroupId,
            boolean isGroupShared,
            @NonNull PointF startPoint,
            float positionX,
            float widthDp) {
        // Return false when another drag in progress.
        if (isDragAlreadyInProgress()) {
            return false;
        }

        // Block drag for last group in single-window mode if feature is not supported.
        boolean allowDragToCreateInstance = shouldAllowGroupDragToCreateInstance(tabGroupId);
        if (!MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity())
                && !allowDragToCreateInstance) {
            return false;
        }

        // Block drag for last tab group when homepage enabled and is set to a custom url.
        if (MultiWindowUtils.getInstance()
                .hasAtMostOneTabGroupWithHomepageEnabled(
                        mTabModelSelector, mCurrentTabGroupModelFilterSupplier.get())) {
            return false;
        }

        // Allow drag to create new instance based on feature checks / current instance count.
        allowDragToCreateInstance =
                allowDragToCreateInstance
                        && (MultiWindowUtils.getInstanceCount()
                                < MultiWindowUtils.getMaxInstances());

        // Extract tab group metadata.
        List<Tab> groupedTabs =
                mCurrentTabGroupModelFilterSupplier.get().getTabsInGroup(tabGroupId);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        TabGroupMetadata metadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        groupedTabs, windowId, mTabModelSelector.getCurrentTabId(), isGroupShared);

        // Build shared state with all info.
        ChromeDropDataAndroid dropData =
                new ChromeTabGroupDropDataAndroid.Builder()
                        .withTabGroupMetadata(metadata)
                        .withAllowDragToCreateInstance(allowDragToCreateInstance)
                        .build();

        // Initialize drag shadow.
        initShadowView(dragSourceView);
        if (mShadowView != null) {
            mShadowView.prepareForGroupDrag(groupedTabs.get(0), (int) (widthDp / mPxToDp));
        }

        return startDragAction(dropData, startPoint, positionX, dragSourceView);
    }

    private boolean startDragAction(
            ChromeDropDataAndroid dropData,
            PointF startPoint,
            float positionX,
            View dragSourceView) {
        DragShadowBuilder builder = createDragShadowBuilder(dragSourceView, startPoint, positionX);
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

    private boolean isDragAlreadyInProgress() {
        if (sDragTrackerToken != null) {
            Log.w(TAG, "Attempting to start drag before clearing state from prior drag");
        }

        return DragDropGlobalState.hasValue();
    }

    private void initShadowView(@NonNull View dragSourceView) {
        // Shadow view is already initialized.
        if (mShadowView != null) return;

        // Create group thumbnail provider.
        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(
                        getActivity(),
                        mBrowserControlStateProvider,
                        mTabContentManagerSupplier.get(),
                        mCurrentTabGroupModelFilterSupplier);
        mMultiThumbnailCardProvider.initWithNative(
                mTabModelSelector.getModel(/* incognito= */ false).getProfile());

        // Inflate/attach the shadow view. Initialize with the required dependencies.
        View rootView =
                View.inflate(
                        dragSourceView.getContext(),
                        R.layout.strip_drag_shadow_view,
                        (ViewGroup) dragSourceView.getRootView());
        mShadowView = rootView.findViewById(R.id.strip_drag_shadow_view);
        mShadowView.initialize(
                mBrowserControlStateProvider,
                mMultiThumbnailCardProvider,
                mTabContentManagerSupplier,
                mLayerTitleCacheSupplier,
                mTabModelSelector,
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

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean res = false;
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                res = onDragStart(dragEvent.getX(), dragEvent.getClipDescription());
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                res = onDragEnd(dragEvent.getResult());
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
                    DragDropMetricUtils.recordDragDropResult(
                            DragDropResult.IGNORED_TOOLBAR,
                            AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                            isTabGroupDrop());
                    res = false;
                }
                break;
        }
        return res;
    }

    /** Sets @{@link TabModelSelector} to retrieve model info. */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        // This supplier will be reset in TabGroupModelFilterProvider#onCurrentTabModelChanged when
        // the tab model switches.
        mCurrentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
    }

    /** Whether a view drag and drop has started. */
    public boolean isViewDraggingInProgress() {
        return sDragTrackerToken != null;
    }

    /** Cleans up internal state. */
    public void destroy() {
        if (mMultiThumbnailCardProvider != null) {
            mMultiThumbnailCardProvider.destroy();
            mMultiThumbnailCardProvider = null;
        }
    }

    private boolean didOccurInTabStrip(float yPx) {
        return yPx <= mTabStripHeightSupplier.get();
    }

    private boolean onDragStart(float xPx, ClipDescription clipDescription) {
        // Only proceed if browser content is being dragged; otherwise, skip the operations.
        if (!MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)
                || DragDropGlobalState.getState(sDragTrackerToken) == null) {
            return false;
        }

        // Return true only when the tab strip is visible.
        // Otherwise, return false to not receive further events until dragEnd.
        if (!isDragSource()) {
            return Boolean.TRUE.equals(mStripLayoutVisibilitySupplier.get());
        }

        // If the tab is quickly dragged off the source strip on drag start with a mouse, the source
        // strip may not receive an enter/exit event, preventing the drag shadow from being made
        // visible. Post an #onDragExit here that will be cancelled if the source strip gets that
        // initial drag enter event.
        // See crbug.com/374480348 for additional info.
        mHandler.postDelayed(mOnDragExitRunnable, /* delayMillis= */ 50L);

        mLastXDp = xPx * mPxToDp;
        return true;
    }

    private boolean onDragEnter(float xPx) {
        mHoveringInStrip = true;
        boolean isDragSource = isDragSource();
        if (isDragSource || XrUtils.isXrDevice()) {
            mHandler.removeCallbacks(mOnDragExitRunnable);
            showDragShadow(false);
        }
        mStripLayoutHelperSupplier
                .get()
                .handleDragEnter(xPx * mPxToDp, mLastXDp, isDragSource, isDraggedItemIncognito());
        return true;
    }

    private boolean onDragLocation(float xPx, float yPx) {
        float xDp = xPx * mPxToDp;
        float yDp = yPx * mPxToDp;
        mStripLayoutHelperSupplier
                .get()
                .handleDragWithin(
                        LayoutManagerImpl.time(),
                        xDp,
                        yDp,
                        xDp - mLastXDp,
                        isDraggedItemIncognito());
        return true;
    }

    private boolean onDrop(DragEvent dropEvent) {
        StripLayoutHelper helper = mStripLayoutHelperSupplier.get();
        helper.stopReorderMode();
        if (isDragSource()) {
            DragDropMetricUtils.recordReorderStripWithDragDrop(
                    mDragEverLeftStrip, isTabGroupDrop());
            return true;
        }

        ClipDescription clipDescription = dropEvent.getClipDescription();
        if (clipDescription == null) return false;

        if (clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB)) {
            return handleTabDrop(dropEvent, helper);
        }

        if (clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP)) {
            return handleGroupDrop(dropEvent, helper);
        }

        return false;
    }

    private boolean handleTabDrop(DragEvent dropEvent, StripLayoutHelper helper) {
        Tab tabBeingDragged =
                ChromeDragDropUtils.getTabFromGlobalState(getDragDropGlobalState(dropEvent));
        if (tabBeingDragged == null) {
            return false;
        }
        boolean tabDraggedBelongToCurrentModel =
                doesBelongToCurrentModel(tabBeingDragged.isIncognitoBranded());

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
            // Reparent tab at drop index and merge to group on destination if needed.
            int tabIndex = helper.getTabIndexForTabDrop(dropEvent.getX() * mPxToDp);
            mMultiInstanceManager.moveTabToWindow(getActivity(), tabBeingDragged, tabIndex);
            helper.maybeMergeToGroupOnDrop(
                    Collections.singletonList(tabBeingDragged.getId()),
                    tabIndex,
                    /* isCollapsed= */ false);
        }
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_TAB_STRIP,
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                /* isTabGroup= */ false);
        return true;
    }

    private boolean handleGroupDrop(DragEvent dropEvent, StripLayoutHelper helper) {
        @Nullable
        TabGroupMetadata tabGroupMetadata =
                ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(
                        getDragDropGlobalState(dropEvent));
        if (tabGroupMetadata == null) {
            return false;
        }

        if (disallowDragWithMhtmlTab(
                mWindowAndroid.getContext().get(), tabGroupMetadata.mhtmlTabTitle)) {
            return false;
        }

        boolean tabGroupDraggedBelongToCurrentModel =
                doesBelongToCurrentModel(tabGroupMetadata.isIncognito);

        // Move tab group to another window.
        if (!tabGroupDraggedBelongToCurrentModel) {
            mMultiInstanceManager.moveTabGroupToWindow(
                    getActivity(),
                    tabGroupMetadata,
                    mTabModelSelector.getModel(tabGroupMetadata.isIncognito).getCount());
            showDroppedDifferentModelToast(mWindowAndroid.getContext().get());
        } else {
            // Reparent tab group at drop index.
            int tabIndex = helper.getTabIndexForTabDrop(dropEvent.getX() * mPxToDp);
            mMultiInstanceManager.moveTabGroupToWindow(getActivity(), tabGroupMetadata, tabIndex);
        }
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_TAB_STRIP,
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                /* isTabGroup= */ true);
        return true;
    }

    private boolean onDragEnd(boolean dropHandled) {
        mHoveringInStrip = false;

        // No-op for destination strip.
        if (!isDragSource()) {
            return false;
        }

        // Get the drag source Chrome instance id before it is cleared as it may be closed.
        int sourceInstanceId =
                DragDropGlobalState.getState(sDragTrackerToken).getDragSourceInstance();

        boolean isTabGroupDrop = isTabGroupDrop();
        mStripLayoutHelperSupplier.get().stopReorderMode();
        mHandler.removeCallbacks(mOnDragExitRunnable);
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
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.SUCCESS,
                    AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                    isTabGroupDrop);
            DragDropMetricUtils.recordDragDropClosedWindow(didCloseWindow, isTabGroupDrop);
        } else if (MultiWindowUtils.getInstanceCount() == MultiWindowUtils.getMaxInstances()) {
            Toast.makeText(
                            mWindowAndroid.getContext().get(),
                            R.string.max_number_of_windows,
                            Toast.LENGTH_LONG)
                    .show();
            ChromeDragDropUtils.recordTabOrGroupDragToCreateInstanceFailureCount();
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_MAX_INSTANCES,
                    AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                    isTabGroupDrop);
        }
        return true;
    }

    private void recordTabRemovedFromGroupUserAction() {
        DragDropGlobalState globalState = DragDropGlobalState.getState(sDragTrackerToken);
        if (globalState.getData() instanceof ChromeTabDropDataAndroid
                && ((ChromeTabDropDataAndroid) globalState.getData()).isTabInGroup) {
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
        }
    }

    private boolean onDragExit() {
        mHoveringInStrip = false;
        mDragEverLeftStrip = true;
        if (XrUtils.isXrDevice()) {
            showDragShadow(true);
        }
        if (isDragSource()) {
            TabDragShadowBuilder builder =
                    (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
            if (builder != null) {
                builder.mShowDragShadow = true;
                mShadowView.expand();
            }
        }
        mStripLayoutHelperSupplier.get().handleDragExit(isDragSource(), isDraggedItemIncognito());
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

    private static DragDropGlobalState getDragDropGlobalState(@Nullable DragEvent dragEvent) {
        return dragEvent != null
                ? DragDropGlobalState.getState(dragEvent)
                : DragDropGlobalState.getState(sDragTrackerToken);
    }

    private boolean isDragSource() {
        DragDropGlobalState globalState = DragDropGlobalState.getState(sDragTrackerToken);
        // May attempt to check source on drag end.
        if (globalState == null) return false;
        return globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId());
    }

    private boolean isDraggedItemIncognito() {
        DragDropGlobalState globalState = DragDropGlobalState.getState(sDragTrackerToken);
        assert globalState != null;

        ChromeDropDataAndroid dropData = (ChromeDropDataAndroid) globalState.getData();
        assert dropData != null;

        return dropData.isIncognito();
    }

    public static boolean canMergeIntoGroupOnDrop() {
        @Nullable
        Tab tab =
                ChromeDragDropUtils.getTabFromGlobalState(
                        getDragDropGlobalState(/* dragEvent= */ null));
        return tab != null;
    }

    private boolean isTabGroupDrop() {
        return ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(
                        getDragDropGlobalState(/* dragEvent= */ null))
                != null;
    }

    public static void setDragTrackerTokenForTesting(TrackerToken token) {
        sDragTrackerToken = token;
        ResettersForTesting.register(() -> sDragTrackerToken = null);
    }

    /**
     * Shows a toast indicating that a tab is dropped into strip in a different model.
     *
     * @param context The context where the toast will be shown.
     */
    private void showDroppedDifferentModelToast(Context context) {
        Toast.makeText(context, R.string.tab_dropped_different_model, Toast.LENGTH_LONG).show();
    }

    /**
     * Checks if the dragged group contains an MHTML tab (identified via a non-null tab title) and
     * shows a toast message to inform the user that the tab cannot be moved.
     *
     * @param context The context to use for showing the toast.
     * @param tabTitle The title of the tab that cannot be moved. If null, no toast is shown.
     * @return {@code true} if the toast was shown, {@code false} otherwise.
     */
    private boolean disallowDragWithMhtmlTab(Context context, @Nullable String tabTitle) {
        if (tabTitle == null) return false;
        String text = context.getString(R.string.tab_cannot_be_moved, tabTitle);
        Toast.makeText(context, text, Toast.LENGTH_LONG).show();
        DragDropMetricUtils.recordDragDropResult(
                DragDropResult.IGNORED_MHTML_TAB,
                AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager),
                /* isTabGroup= */ true);
        return true;
    }

    private boolean doesBelongToCurrentModel(boolean draggedIncognito) {
        return mTabModelSelector.getCurrentModel().isIncognitoBranded() == draggedIncognito;
    }

    private Activity getActivity() {
        assert mWindowAndroid.getActivity().get() != null;
        return mWindowAndroid.getActivity().get();
    }

    @VisibleForTesting
    static class TabDragShadowBuilder extends View.DragShadowBuilder {
        // Touch offset for drag shadow view.
        private final PointF mDragShadowOffset;
        // Source initiating drag - to call updateDragShadow().
        private final View mDragSourceView;
        // Whether drag shadow should be shown.
        private boolean mShowDragShadow;

        public TabDragShadowBuilder(View dragSourceView, View shadowView, PointF dragShadowOffset) {
            // Store the View parameter.
            super(shadowView);
            mDragShadowOffset = dragShadowOffset;
            mDragSourceView = dragSourceView;
        }

        public void update(boolean show) {
            mShowDragShadow = show;
            mDragSourceView.updateDragShadow(this);
        }

        @Override
        public void onDrawShadow(@NonNull Canvas canvas) {
            View shadowView = getView();
            if (mShowDragShadow) {
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

        // Set the touch point of the drag shadow:
        // Horizontally matching user's touch point within the tab title;
        // Vertically centered in the tab title.
        Resources resources = dragSourceView.getContext().getResources();
        float dragShadowOffsetY =
                resources.getDimension(R.dimen.tab_grid_card_header_height) / 2
                        + resources.getDimension(R.dimen.tab_grid_card_margin);
        dragShadowOffset = new PointF((startPoint.x - tabPositionX) / mPxToDp, dragShadowOffsetY);
        return new TabDragShadowBuilder(dragSourceView, mShadowView, dragShadowOffset);
    }

    private boolean shouldAllowGroupDragToCreateInstance(Token groupId) {
        int groupSize = mCurrentTabGroupModelFilterSupplier.get().getTabCountForGroup(groupId);

        return mTabModelSelector.getTotalTabCount() > groupSize;
    }

    private boolean shouldAllowTabDragToCreateInstance() {
        return hasMultipleTabs(mTabModelSelector);
    }

    private boolean hasMultipleTabs(TabModelSelector tabModelSelector) {
        return tabModelSelector != null && tabModelSelector.getTotalTabCount() > 1;
    }

    View getShadowViewForTesting() {
        return mShadowView;
    }

    Handler getHandlerForTesting() {
        return mHandler;
    }

    Runnable getOnDragExitRunnableForTesting() {
        return mOnDragExitRunnable;
    }
}
