// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.isTabPinningFromStripEnabled;

import android.app.Activity;
import android.content.ClipDescription;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Handler;
import android.os.Looper;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.dragdrop.ChromeDragDropUtils;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/**
 * Manages initiating tab drag and drop and handles the events that are received during drag and
 * drop process. The tab drag and drop is initiated from the active instance of {@link
 * StripLayoutHelper}.
 */
@NullMarked
public class TabStripDragHandler extends TabDragHandlerBase {
    private static final String TAG = "TabStripDragHandler";

    private final Supplier<StripLayoutHelper> mStripLayoutHelperSupplier;
    private final Supplier<Boolean> mStripLayoutVisibilitySupplier;
    private final ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final ObservableSupplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private final BrowserControlsStateProvider mBrowserControlStateProvider;
    private final float mPxToDp;
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;

    /** Handler and runnable to post/cancel an #onDragExit when the drag starts. */
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final Runnable mOnDragExitRunnable = this::onDragExit;

    /** Drag shadow properties */
    @Nullable private StripDragShadowView mShadowView;

    @Nullable private MultiThumbnailCardProvider mMultiThumbnailCardProvider;

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
     * @param activitySupplier Supplier for the current activity.
     * @param tabStripHeightSupplier Supplier of the tab strip height.
     * @param isAppInDesktopWindowSupplier Supplier for the current window desktop state.
     */
    public TabStripDragHandler(
            Context context,
            Supplier<StripLayoutHelper> stripLayoutHelperSupplier,
            Supplier<Boolean> stripLayoutVisibilitySupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            ObservableSupplier<LayerTitleCache> layerTitleCacheSupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            BrowserControlsStateProvider browserControlStateProvider,
            Supplier<@Nullable Activity> activitySupplier,
            ObservableSupplier<Integer> tabStripHeightSupplier,
            Supplier<Boolean> isAppInDesktopWindowSupplier) {
        super(
                activitySupplier,
                multiInstanceManager,
                dragAndDropDelegate,
                isAppInDesktopWindowSupplier);
        mPxToDp = 1.f / context.getResources().getDisplayMetrics().density;
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mStripLayoutHelperSupplier = stripLayoutHelperSupplier;
        mStripLayoutVisibilitySupplier = stripLayoutVisibilitySupplier;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mBrowserControlStateProvider = browserControlStateProvider;
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
            View dragSourceView,
            Tab tabBeingDragged,
            PointF startPoint,
            float tabPositionX,
            float tabWidthDp) {
        if (!canStartTabDrag()) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareTabDropData(tabBeingDragged);

        // Initialize drag shadow.
        initShadowView(dragSourceView);
        if (mShadowView != null) {
            mShadowView.prepareForTabDrag(tabBeingDragged, (int) (tabWidthDp / mPxToDp));
        }
        return startDragInternal(dropData, startPoint, tabPositionX, dragSourceView);
    }

    /**
     * Starts the multi-tab drag action by initiating the process by calling View.startDragAndDrop.
     *
     * @param dragSourceView View used to create the drag shadow.
     * @param tabsBeingDragged List of {@link Tab}s being dragged.
     * @param primaryTab The primary {@link Tab} that the user is interacting with.
     * @param startPoint Position of the drag start point in view coordinates.
     * @param tabPositionX Horizontal position of the dragged tab in view coordinates. Used to
     *     calculate the relative position of the touch point in the tab strip.
     * @param tabWidthDp Width of the source strip tab container in dp.
     * @return true if the drag action was initiated successfully.
     */
    public boolean startMultiTabDragAction(
            View dragSourceView,
            List<Tab> tabsBeingDragged,
            Tab primaryTab,
            PointF startPoint,
            float tabPositionX,
            float tabWidthDp) {
        if (!canStartMultiTabDrag()) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareMultiTabDropData(tabsBeingDragged, primaryTab);

        // Initialize drag shadow.
        initShadowView(dragSourceView);
        if (mShadowView != null) {
            mShadowView.prepareForMultiTabDrag(
                    primaryTab, tabsBeingDragged, (int) (tabWidthDp / mPxToDp));
        }
        return startDragInternal(dropData, startPoint, tabPositionX, dragSourceView);
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
            View dragSourceView,
            Token tabGroupId,
            boolean isGroupShared,
            PointF startPoint,
            float positionX,
            float widthDp) {
        if (!canStartGroupDrag(tabGroupId)) {
            return false;
        }

        ChromeDropDataAndroid dropData = prepareGroupDropData(tabGroupId, isGroupShared);

        // Initialize drag shadow.
        initShadowView(dragSourceView);
        if (mShadowView != null) {
            Tab firstTabInGroup = getCurrentTabGroupModelFilter().getTabsInGroup(tabGroupId).get(0);
            mShadowView.prepareForGroupDrag(firstTabInGroup, (int) (widthDp / mPxToDp));
        }
        return startDragInternal(dropData, startPoint, positionX, dragSourceView);
    }

    private boolean startDragInternal(
            ChromeDropDataAndroid dropData,
            PointF startPoint,
            float positionX,
            View dragSourceView) {
        DragShadowBuilder builder = createDragShadowBuilder(dragSourceView, startPoint, positionX);
        return startDrag(dragSourceView, builder, dropData);
    }

    private void initShadowView(View dragSourceView) {
        // Shadow view is already initialized.
        if (mShadowView != null) return;

        // Create group thumbnail provider.
        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        assert tabContentManager != null;
        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(
                        getActivity(),
                        mBrowserControlStateProvider,
                        tabContentManager,
                        getCurrentTabGroupModelFilterSupplier());
        mMultiThumbnailCardProvider.initWithNative(
                assumeNonNull(getTabModelSelector().getModel(/* incognito= */ false).getProfile()));

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
                tabContentManager,
                mLayerTitleCacheSupplier,
                getTabModelSelector(),
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
                            mIsAppInDesktopWindowSupplier.get(),
                            isTabGroupDrop(),
                            isMultiTabDrop());
                    res = false;
                }
                break;
        }
        return res;
    }

    /** Cleans up internal state. */
    @Override
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
        if (!isDraggingBrowserContent(clipDescription)) {
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
        if (isDragSource || DeviceInfo.isXr()) {
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
                .handleDragWithin(xDp, yDp, xDp - mLastXDp, isDraggedItemIncognito());
        return true;
    }

    private boolean onDrop(DragEvent dropEvent) {
        StripLayoutHelper helper = mStripLayoutHelperSupplier.get();
        helper.stopReorderMode();
        if (isDragSource()) {
            DragDropMetricUtils.recordReorderStripWithDragDrop(
                    mDragEverLeftStrip, isTabGroupDrop(), isMultiTabDrop());
            return true;
        }

        ClipDescription clipDescription = dropEvent.getClipDescription();
        if (clipDescription == null) return false;

        if (clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB)) {
            return handleTabDrop(dropEvent, helper);
        }

        if (clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB)) {
            return handleMultiTabDrop(dropEvent, helper);
        }

        if (clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP)) {
            return handleGroupDrop(dropEvent, helper);
        }

        return false;
    }

    private boolean handleTabDrop(DragEvent dropEvent, StripLayoutHelper helper) {
        DragDropGlobalState globalState = getDragDropGlobalState(dropEvent);
        assertNonNull(globalState);
        Tab tabBeingDragged = ChromeDragDropUtils.getTabFromGlobalState(globalState);
        if (tabBeingDragged == null) {
            return false;
        }
        boolean tabDraggedBelongToCurrentModel =
                doesBelongToCurrentModel(tabBeingDragged.isIncognitoBranded());

        // Record user action if a grouped tab is going to be re-parented.
        recordTabRemovedFromGroupUserAction();

        // Move tab to another window.
        if (!tabDraggedBelongToCurrentModel) {
            // Reject cross-model drops if incognito is opened as a new window.
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) return false;

            mMultiInstanceManager.moveTabsToWindow(
                    getActivity(),
                    Collections.singletonList(tabBeingDragged),
                    getTabModelSelector().getModel(tabBeingDragged.isIncognito()).getCount());
            showDroppedDifferentModelToast(getActivity());
        } else {
            // Reparent tab at drop index and merge to group on destination if needed.
            int tabIndex =
                    helper.getTabIndexForTabDrop(
                            dropEvent.getX() * mPxToDp, tabBeingDragged.getIsPinned());
            mMultiInstanceManager.moveTabsToWindow(
                    getActivity(), Collections.singletonList(tabBeingDragged), tabIndex);
            helper.maybeMergeToGroupOnDrop(
                    Collections.singletonList(tabBeingDragged.getId()),
                    tabIndex,
                    /* isCollapsed= */ false);
        }
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_TAB_STRIP,
                mIsAppInDesktopWindowSupplier.get(),
                /* isTabGroup= */ false,
                /* isMultiTab= */ false);
        return true;
    }

    // TODO(crbug.com/437417213): Handle pinned tab.
    private boolean handleMultiTabDrop(DragEvent dropEvent, StripLayoutHelper helper) {
        DragDropGlobalState globalState = getDragDropGlobalState(dropEvent);
        assertNonNull(globalState);
        List<Tab> tabsBeingDragged = ChromeDragDropUtils.getTabsFromGlobalState(globalState);
        if (tabsBeingDragged == null || tabsBeingDragged.isEmpty()) {
            return false;
        }
        boolean tabsDraggedBelongToCurrentModel =
                doesBelongToCurrentModel(tabsBeingDragged.get(0).isIncognitoBranded());
        // Move tabs to another window.
        if (!tabsDraggedBelongToCurrentModel) {
            // Reject cross-model drops if incognito is opened as a new window.
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) return false;

            mMultiInstanceManager.moveTabsToWindow(
                    getActivity(),
                    tabsBeingDragged,
                    getTabModelSelector()
                            .getModel(tabsBeingDragged.get(0).isIncognito())
                            .getCount());
            showDroppedDifferentModelToast(getActivity());
        } else {
            // Reparent tabs at drop index.
            int tabIndex =
                    helper.getTabIndexForTabDrop(
                            dropEvent.getX() * mPxToDp, isDraggingPinnedItem());
            mMultiInstanceManager.moveTabsToWindow(getActivity(), tabsBeingDragged, tabIndex);
            List<Integer> tabsBeingDraggedIds = new ArrayList<>();
            for (Tab tab : tabsBeingDragged) {
                tabsBeingDraggedIds.add(tab.getId());
            }
            helper.maybeMergeToGroupOnDrop(tabsBeingDraggedIds, tabIndex, /* isCollapsed= */ false);
        }
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_TAB_STRIP,
                mIsAppInDesktopWindowSupplier.get(),
                /* isTabGroup= */ false,
                /* isMultiTab= */ true);
        return true;
    }

    private boolean handleGroupDrop(DragEvent dropEvent, StripLayoutHelper helper) {
        DragDropGlobalState globalState = getDragDropGlobalState(dropEvent);
        assertNonNull(globalState);
        @Nullable TabGroupMetadata tabGroupMetadata =
                ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState);
        if (tabGroupMetadata == null) {
            return false;
        }

        if (disallowDragWithMhtmlTab(getActivity(), tabGroupMetadata.mhtmlTabTitle)) {
            return false;
        }

        boolean tabGroupDraggedBelongToCurrentModel =
                doesBelongToCurrentModel(tabGroupMetadata.isIncognito);

        // Move tab group to another window.
        if (!tabGroupDraggedBelongToCurrentModel) {
            // Reject cross-model drops if incognito is opened as a new window.
            if (IncognitoUtils.shouldOpenIncognitoAsWindow()) return false;

            mMultiInstanceManager.moveTabGroupToWindow(
                    getActivity(),
                    tabGroupMetadata,
                    getTabModelSelector().getModel(tabGroupMetadata.isIncognito).getCount());
            showDroppedDifferentModelToast(getActivity());
        } else {
            // Reparent tab group at drop index.
            int tabIndex =
                    helper.getTabIndexForTabDrop(dropEvent.getX() * mPxToDp, /* isPinned= */ false);
            mMultiInstanceManager.moveTabGroupToWindow(getActivity(), tabGroupMetadata, tabIndex);
        }
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_TAB_STRIP,
                mIsAppInDesktopWindowSupplier.get(),
                /* isTabGroup= */ true,
                /* isMultiTab= */ false);
        return true;
    }

    private boolean onDragEnd(boolean dropHandled) {
        mHoveringInStrip = false;

        // No-op for destination strip.
        if (!isDragSource()) {
            return false;
        }

        mStripLayoutHelperSupplier.get().stopReorderMode();
        mHandler.removeCallbacks(mOnDragExitRunnable);
        if (mShadowView != null) {
            mShadowView.clear();
        }

        finishDrag(dropHandled);

        return true;
    }

    private void recordTabRemovedFromGroupUserAction() {
        DragDropGlobalState globalState = getDragDropGlobalState(null);
        if (globalState != null
                && globalState.getData() instanceof ChromeTabDropDataAndroid
                && ((ChromeTabDropDataAndroid) globalState.getData()).isTabInGroup) {
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
        }
    }

    private boolean onDragExit() {
        mHoveringInStrip = false;
        mDragEverLeftStrip = true;
        if (DeviceInfo.isXr()) {
            showDragShadow(true);
        }
        boolean isDragSource = isDragSource();
        if (isDragSource) {
            TabDragShadowBuilder builder =
                    (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
            if (builder != null && mShadowView != null) {
                builder.mShowDragShadow = true;
                mShadowView.expand();
            }
        }
        mStripLayoutHelperSupplier.get().handleDragExit(isDragSource, isDraggedItemIncognito());
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

    public static boolean isDraggingUnpinnedTab() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        assertNonNull(globalState);

        Tab tab = ChromeDragDropUtils.getTabFromGlobalState(globalState);
        if (tab != null && !tab.getIsPinned()) return true;

        Tab primaryTab = ChromeDragDropUtils.getPrimaryTabFromGlobalState(globalState);
        return primaryTab != null && !primaryTab.getIsPinned();
    }

    public static boolean isDraggingPinnedItem() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        if (!isTabPinningFromStripEnabled() || globalState == null) return false;

        Tab tab = ChromeDragDropUtils.getTabFromGlobalState(globalState);
        if (tab != null && tab.getIsPinned()) return true;

        List<Tab> tabs = ChromeDragDropUtils.getTabsFromGlobalState(globalState);
        if (tabs == null) return false;

        for (Tab curTab : tabs) {
            if (!curTab.getIsPinned()) {
                return false;
            }
        }
        return true;
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
                mIsAppInDesktopWindowSupplier.get(),
                /* isTabGroup= */ true,
                /* isMultiTab= */ false);
        return true;
    }

    @VisibleForTesting
    static class TabDragShadowBuilder extends View.DragShadowBuilder {
        // Touch offset for drag shadow view.
        private final PointF mDragShadowOffset;
        // Source initiating drag - to call updateDragShadow().
        private final View mDragSourceView;
        // Whether drag shadow should be shown.
        private boolean mShowDragShadow;
        // Paint for the shadow.
        private final Paint mShadowPaint;
        private final float mCornerRadius;

        public TabDragShadowBuilder(View dragSourceView, View shadowView, PointF dragShadowOffset) {
            // Store the View parameter.
            super(shadowView);
            mDragShadowOffset = dragShadowOffset;
            mDragSourceView = dragSourceView;

            // Set up the shadow paint.
            Context context = shadowView.getContext();
            Resources resources = shadowView.getResources();
            mShadowPaint = new Paint();
            mShadowPaint.setAntiAlias(true);
            mShadowPaint.setColor(context.getColor(R.color.tab_strip_reorder_shadow_color));
            float blurThickness =
                    resources.getDimension(R.dimen.tab_strip_dragged_tab_shadow_thickness);
            mShadowPaint.setMaskFilter(
                    new BlurMaskFilter(blurThickness, BlurMaskFilter.Blur.OUTER));
            mCornerRadius = resources.getDimension(R.dimen.tab_grid_card_bg_radius);
        }

        public void update(boolean show) {
            mShowDragShadow = show;
            mDragSourceView.updateDragShadow(this);
        }

        @Override
        public void onDrawShadow(Canvas canvas) {
            View shadowView = getView();
            if (mShowDragShadow) {
                View cardView = shadowView.findViewById(R.id.card_view);
                if (cardView == null) {
                    shadowView.draw(canvas); // Fallback
                    return;
                }
                // Draw the shadow.
                canvas.drawRoundRect(
                        cardView.getLeft(),
                        cardView.getTop(),
                        cardView.getRight(),
                        cardView.getBottom(),
                        mCornerRadius,
                        mCornerRadius,
                        mShadowPaint);

                // Draw the view on top of the shadow.
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
        assert mShadowView != null;
        return new TabDragShadowBuilder(dragSourceView, mShadowView, dragShadowOffset);
    }

    @Nullable View getShadowViewForTesting() {
        return mShadowView;
    }

    Handler getHandlerForTesting() {
        return mHandler;
    }

    Runnable getOnDragExitRunnableForTesting() {
        return mOnDragExitRunnable;
    }
}
