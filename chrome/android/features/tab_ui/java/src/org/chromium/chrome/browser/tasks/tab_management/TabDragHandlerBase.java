// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.ClipDescription;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.dragdrop.ChromeDragDropUtils;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeMultiTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropGlobalState.TrackerToken;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;

import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** A helper class that provides access to common logic involved in tab dragging. */
@NullMarked
public abstract class TabDragHandlerBase implements View.OnDragListener, Destroyable {
    private static final String TAG = "TabDragHandlerBase";
    private static @Nullable TrackerToken sDragTrackerToken;

    private final Supplier<@Nullable Activity> mActivitySupplier;

    protected final MultiInstanceManager mMultiInstanceManager;
    protected final DragAndDropDelegate mDragAndDropDelegate;
    protected final Supplier<Boolean> mIsAppInDesktopWindowSupplier;
    protected @Nullable ObservableSupplier<Boolean> mFullSpaceModeSupplier;
    protected @Nullable Callback<Boolean> mFullSpaceModeObserver;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;

    /**
     * Prepares the tab container view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param activitySupplier Supplier for the current activity.
     * @param multiInstanceManager {@link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     * @param isAppInDesktopWindowSupplier Supplier for the current window desktop state.
     */
    public TabDragHandlerBase(
            Supplier<@Nullable Activity> activitySupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            Supplier<Boolean> isAppInDesktopWindowSupplier) {
        mActivitySupplier = activitySupplier;
        mMultiInstanceManager = multiInstanceManager;
        mDragAndDropDelegate = dragAndDropDelegate;
        mIsAppInDesktopWindowSupplier = isAppInDesktopWindowSupplier;
    }

    /** Sets @{@link TabModelSelector} to retrieve model info. */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
        mCurrentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
    }

    /** Whether a view drag and drop has started. */
    public boolean isViewDraggingInProgress() {
        return sDragTrackerToken != null;
    }

    @Override
    public void destroy() {
        // Not implemented.
    }

    protected Activity getActivity() {
        @Nullable Activity activity = mActivitySupplier.get();
        assert activity != null;
        return activity;
    }

    protected TabModelSelector getTabModelSelector() {
        assert mTabModelSelector != null;
        return mTabModelSelector;
    }

    protected ObservableSupplier<@Nullable TabGroupModelFilter>
            getCurrentTabGroupModelFilterSupplier() {
        assert mCurrentTabGroupModelFilterSupplier != null;
        return mCurrentTabGroupModelFilterSupplier;
    }

    protected TabGroupModelFilter getCurrentTabGroupModelFilter() {
        @Nullable TabGroupModelFilter filter = getCurrentTabGroupModelFilterSupplier().get();
        assert filter != null;
        return filter;
    }

    protected boolean canStartTabDrag() {
        if (isDragAlreadyInProgress()) {
            return false;
        }

        // Block drag for last tab in single-window mode if feature is not supported.
        if (!MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity())
                && !shouldAllowTabDragToCreateInstance()) {
            return false;
        }

        // Block drag for last tab when homepage enabled and is set to a custom url.
        if (MultiWindowUtils.getInstance()
                .hasAtMostOneTabWithHomepageEnabled(getTabModelSelector())) {
            return false;
        }

        return true;

    }

    protected boolean canStartMultiTabDrag() {
        if (isDragAlreadyInProgress()) {
            return false;
        }

        // Block drag for last tab in single-window mode if feature is not supported.
        if (!MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity())
                && !shouldAllowMultiTabDragToCreateInstance()) {
            return false;
        }

        // Block drag for last tab when homepage enabled and is set to a custom url.
        if (MultiWindowUtils.getInstance()
                .hasAllTabsSelectedWithHomepageEnabled(getTabModelSelector())) {
            return false;
        }

        return true;
    }

    protected boolean canStartGroupDrag(Token tabGroupId) {
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
                        getTabModelSelector(), getCurrentTabGroupModelFilter())) {
            return false;
        }

        return true;
    }

    private boolean shouldAllowGroupDragToCreateInstance(Token groupId) {
        int groupSize = getCurrentTabGroupModelFilter().getTabCountForGroup(groupId);
        return getTabModelSelector().getTotalTabCount() > groupSize;
    }

    private boolean shouldAllowMultiTabDragToCreateInstance() {
        return getTabModelSelector().getTotalTabCount()
                > getTabModelSelector().getCurrentModel().getMultiSelectedTabsCount();
    }

    private boolean shouldAllowTabDragToCreateInstance() {
        return getTabModelSelector().getTotalTabCount() > 1;
    }

    protected boolean isDragAlreadyInProgress() {
        if (sDragTrackerToken != null) {
            Log.w(TAG, "Attempting to start drag before clearing state from prior drag");
        }

        return DragDropGlobalState.hasValue();
    }

    protected boolean isDraggingBrowserContent(ClipDescription clipDescription) {
        // Only proceed if browser content is being dragged; otherwise, skip the operations.
        return MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)
                && getDragDropGlobalState(null) != null;
    }

    protected boolean isDragSource() {
        DragDropGlobalState globalState = getDragDropGlobalState(null);
        // May attempt to check source on drag end.
        if (globalState == null) return false;
        return globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId());
    }

    protected boolean isDraggedItemIncognito() {
        DragDropGlobalState globalState = getDragDropGlobalState(null);
        assert globalState != null;

        ChromeDropDataAndroid dropData = (ChromeDropDataAndroid) globalState.getData();
        assert dropData != null;

        return dropData.isIncognito();
    }

    protected boolean isTabGroupDrop() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        assertNonNull(globalState);
        return ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState) != null;
    }

    protected boolean isMultiTabDrop() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        assertNonNull(globalState);
        return ChromeDragDropUtils.getTabsFromGlobalState(globalState) != null;
    }

    protected boolean doesBelongToCurrentModel(boolean draggedIncognito) {
        return getTabModelSelector().getCurrentModel().isIncognitoBranded() == draggedIncognito;
    }

    protected ChromeDropDataAndroid prepareTabDropData(Tab tab) {
        boolean isTabInGroup = getCurrentTabGroupModelFilter().isTabInTabGroup(tab);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        boolean allowDragToCreateInstance =
                shouldAllowTabDragToCreateInstance()
                        && (TabUiFeatureUtilities.doesOemSupportDragToCreateInstance()
                                || MultiWindowUtils.getInstanceCountWithFallback(
                                                PersistedInstanceType.ACTIVE)
                                        < MultiWindowUtils.getMaxInstances());

        return new ChromeTabDropDataAndroid.Builder()
                .withTab(tab)
                .withTabInGroup(isTabInGroup)
                .withAllowDragToCreateInstance(allowDragToCreateInstance)
                .withWindowId(windowId)
                .build();
    }

    protected ChromeDropDataAndroid prepareMultiTabDropData(List<Tab> tabs, Tab primaryTab) {
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        boolean allowDragToCreateInstance =
                shouldAllowMultiTabDragToCreateInstance()
                        && (TabUiFeatureUtilities.doesOemSupportDragToCreateInstance()
                                || MultiWindowUtils.getInstanceCountWithFallback(
                                                PersistedInstanceType.ACTIVE)
                                        < MultiWindowUtils.getMaxInstances());

        ChromeMultiTabDropDataAndroid.Builder builder = new ChromeMultiTabDropDataAndroid.Builder();
        builder.withAllowDragToCreateInstance(allowDragToCreateInstance);
        builder.withWindowId(windowId);
        // Reverse the order to preserve the order in the destination strip.
        Collections.reverse(tabs);
        builder.withTabs(tabs).withPrimaryTab(primaryTab);
        return builder.build();
    }

    protected ChromeDropDataAndroid prepareGroupDropData(Token tabGroupId, boolean isGroupShared) {
        TabGroupModelFilter filter = getCurrentTabGroupModelFilter();
        List<Tab> groupedTabs = filter.getTabsInGroup(tabGroupId);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        TabGroupMetadata metadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        filter,
                        groupedTabs,
                        windowId,
                        getTabModelSelector().getCurrentTabId(),
                        isGroupShared);
        boolean allowDragToCreateInstance =
                shouldAllowGroupDragToCreateInstance(tabGroupId)
                        && (MultiWindowUtils.getInstanceCountWithFallback(
                                        PersistedInstanceType.ACTIVE)
                                < MultiWindowUtils.getMaxInstances());

        ChromeTabGroupDropDataAndroid.Builder builder = new ChromeTabGroupDropDataAndroid.Builder();
        builder.withAllowDragToCreateInstance(allowDragToCreateInstance);

        if (metadata != null) {
            builder.withTabGroupMetadata(metadata);
        }

        return builder.build();
    }

    /**
     * Start drag by creating a new global state token and invoking the {@link DragAndDropDelegate}.
     *
     * @param dragSourceView {@link View} that initiated drag.
     * @param builder {@link DragShadowBuilder} to build a drag shadow.
     * @param dropData A {@link ChromeDropDataAndroid} object pointing to the data to be transferred
     *     by the drag and drop operation.
     */
    protected boolean startDrag(
            View dragSourceView, DragShadowBuilder builder, ChromeDropDataAndroid dropData) {
        sDragTrackerToken =
                DragDropGlobalState.store(
                        mMultiInstanceManager.getCurrentInstanceId(), dropData, builder);
        boolean res = mDragAndDropDelegate.startDragAndDrop(dragSourceView, builder, dropData);
        if (!res) {
            clearDragDropGlobalState();
        }
        return res;
    }

    /**
     * Finish the drag by moving the tab to a new window if needed.
     *
     * @param dropHandled true if the dragEvent was already handled, false otherwise.
     */
    protected void finishDrag(boolean dropHandled) {
        // Get the drag source Chrome instance id before it is cleared as it may be closed.
        @Nullable DragDropGlobalState dragDropGlobalState = getDragDropGlobalState(null);
        int sourceInstanceId =
                dragDropGlobalState != null
                        ? dragDropGlobalState.getDragSourceInstance()
                        : INVALID_WINDOW_ID;
        boolean isTabGroupDrop = isTabGroupDrop();
        boolean isMultiTabDrop = isMultiTabDrop();

        clearDragDropGlobalState();

        // Close the source instance window if it has no tabs.
        boolean didCloseWindow = mMultiInstanceManager.closeChromeWindowIfEmpty(sourceInstanceId);

        // Only record for source strip to avoid duplicate.
        if (dropHandled) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.SUCCESS,
                    mIsAppInDesktopWindowSupplier.get(),
                    isTabGroupDrop,
                    isMultiTabDrop);
            DragDropMetricUtils.recordDragDropClosedWindow(
                    didCloseWindow, isTabGroupDrop, isMultiTabDrop);
        } else if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE)
                >= MultiWindowUtils.getMaxInstances()) {
            assumeNonNull(mTabModelSelector);
            assumeNonNull(mTabModelSelector.getCurrentTab());
            var windowAndroid = mTabModelSelector.getCurrentTab().getWindowAndroid();
            mMultiInstanceManager.showInstanceCreationLimitMessage(
                    MessageDispatcherProvider.from(windowAndroid));
            ChromeDragDropUtils.recordTabOrGroupDragToCreateInstanceFailureCount();
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_MAX_INSTANCES,
                    mIsAppInDesktopWindowSupplier.get(),
                    isTabGroupDrop,
                    isMultiTabDrop);
        }
    }

    @Nullable
    protected Tab getTabFromGlobalState(@Nullable DragEvent dragEvent) {
        DragDropGlobalState globalState = getDragDropGlobalState(dragEvent);
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        if (!(globalState.getData() instanceof ChromeTabDropDataAndroid)) return null;
        return ((ChromeTabDropDataAndroid) globalState.getData()).tab;
    }

    protected void clearDragDropGlobalState() {
        if (sDragTrackerToken != null) {
            DragDropGlobalState.clear(sDragTrackerToken);
            sDragTrackerToken = null;
        }
    }

    @Nullable
    protected static DragDropGlobalState getDragDropGlobalState(@Nullable DragEvent dragEvent) {
        if (dragEvent != null) {
            return DragDropGlobalState.getState(dragEvent);
        }
        if (sDragTrackerToken != null) {
            return DragDropGlobalState.getState(sDragTrackerToken);
        }
        return null;
    }

    public static void setDragTrackerTokenForTesting(TrackerToken token) {
        sDragTrackerToken = token;
        ResettersForTesting.register(() -> sDragTrackerToken = null);
    }
}
