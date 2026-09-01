// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.ClipDescription;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
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
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDragStateData;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DropDataAndroid;

import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** A helper class that provides access to common logic involved in tab dragging. */
@NullMarked
public abstract class TabDragHandlerBase
        implements View.OnDragListener, Destroyable, BackPressHandler {
    private static final String TAG = "TabDragHandlerBase";
    private static @Nullable Token sDragToken;

    private final Supplier<@Nullable Activity> mActivitySupplier;
    private final SettableNonNullObservableSupplier<Boolean> mDragInProgressSupplier =
            ObservableSuppliers.createNonNull(false);
    protected final MultiInstanceManager mMultiInstanceManager;
    protected final MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    protected final DragAndDropDelegate mDragAndDropDelegate;
    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable MonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private @Nullable TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    protected @Nullable View mDragSourceView;

    /**
     * Prepares the tab container view to listen to the drag events and data drop after the drag is
     * initiated.
     *
     * @param activitySupplier Supplier for the current activity.
     * @param multiInstanceManager {@link MultiInstanceManager} to perform move action when drop
     *     completes.
     * @param dragAndDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     */
    public TabDragHandlerBase(
            Supplier<@Nullable Activity> activitySupplier,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate) {
        mActivitySupplier = activitySupplier;
        mMultiInstanceManager = multiInstanceManager;
        mMultiInstanceOrchestrator = MultiInstanceOrchestratorFactory.getInstance();
        mDragAndDropDelegate = dragAndDropDelegate;
    }

    /** Sets @{@link TabModelSelector} to retrieve model info. */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert mTabModelSelector == null;
        mTabModelSelector = tabModelSelector;
        mCurrentTabModelSupplier = mTabModelSelector.getCurrentTabModelSupplier();
        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mTabModelSelector) {
                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        onTabsClosed(Collections.singletonList(tab));
                    }

                    @Override
                    public void willCloseTabs(
                            List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {
                        onTabsClosed(tabs);
                    }
                };
    }

    /** Whether a view drag and drop has started. */
    public boolean isViewDraggingInProgress() {
        return sDragToken != null;
    }

    @Override
    public void destroy() {
        if (mTabModelSelectorTabModelObserver != null) {
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabModelObserver = null;
        }
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

    protected MonotonicObservableSupplier<TabModel> getCurrentTabModelSupplier() {
        assert mCurrentTabModelSupplier != null;
        return mCurrentTabModelSupplier;
    }

    protected TabModel getCurrentModel() {
        return getTabModelSelector().getCurrentModel();
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
                        getTabModelSelector(), getCurrentModel())) {
            return false;
        }

        return true;
    }

    private boolean shouldAllowGroupDragToCreateInstance(Token groupId) {
        int groupSize = getCurrentModel().getTabCountForGroup(groupId);
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
        if (sDragToken != null) {
            Log.w(TAG, "Attempting to start drag before clearing state from prior drag");
        }

        return DragDropGlobalState.hasValue();
    }

    protected boolean isDraggingBrowserContent(ClipDescription clipDescription) {
        // Only proceed if browser content is being dragged; otherwise, skip the operations.
        return MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)
                && getDragDropGlobalState(null) != null;
    }

    /** Returns whether this handler instance initiated the active drag operation. */
    public boolean isDragSource() {
        // If this handler instance did not initiate the drag, it is not the drag source.
        if (mDragSourceView == null) return false;

        DragDropGlobalState globalState = getDragDropGlobalState(null);
        // May attempt to check source on drag end.
        if (globalState == null) return false;
        return globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId());
    }

    /** Returns whether the active drag operation was initiated by this Chrome window instance. */
    public boolean isDragSourceInstance() {
        DragDropGlobalState globalState = getDragDropGlobalState(null);
        if (globalState == null) return false;
        return globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId());
    }

    /** Returns whether the item currently being dragged is incognito branded. */
    public boolean isDraggedItemIncognito() {
        DragDropGlobalState globalState = getDragDropGlobalState(null);
        if (globalState == null) return false;

        if (globalState.getData() instanceof ChromeDropDataAndroid dropData) {
            return dropData.isIncognito();
        }
        return false;
    }

    protected boolean isTabGroupDrop() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        if (globalState == null) return false;
        return ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState) != null;
    }

    protected boolean isMultiTabDrop() {
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        if (globalState == null) return false;
        return ChromeDragDropUtils.getTabsFromGlobalState(globalState) != null;
    }

    /**
     * Returns whether the incognito state of a dragged item matches the current tab model.
     *
     * @param draggedIncognito True if the dragged item is incognito, false otherwise.
     * @return True if the dragged item belongs to the active {@link TabModel}, false otherwise.
     */
    public boolean doesBelongToCurrentModel(boolean draggedIncognito) {
        if (mTabModelSelector == null) return false;
        TabModel currentModel = mTabModelSelector.getCurrentModel();
        if (currentModel == null) return false;
        return currentModel.isIncognitoBranded() == draggedIncognito;
    }

    protected ChromeDropDataAndroid prepareTabDropData(Tab tab) {
        boolean isTabInGroup = getCurrentModel().isTabInTabGroup(tab);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        boolean allowDragToCreateInstance =
                shouldAllowTabDragToCreateInstance()
                        && (TabUiFeatureUtilities.doesOemSupportDragToCreateInstance()
                                || MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE)
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
                                || MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE)
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
        TabModel tabModel = getCurrentModel();
        List<Tab> groupedTabs = tabModel.getTabsInGroup(tabGroupId);
        int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(getActivity());
        TabGroupMetadata metadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        tabModel,
                        groupedTabs,
                        windowId,
                        getTabModelSelector().getCurrentTabId(),
                        isGroupShared);
        boolean allowDragToCreateInstance =
                shouldAllowGroupDragToCreateInstance(tabGroupId)
                        && (MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE)
                                < MultiWindowUtils.getMaxInstances());

        ChromeTabGroupDropDataAndroid.Builder builder = new ChromeTabGroupDropDataAndroid.Builder();
        builder.withAllowDragToCreateInstance(allowDragToCreateInstance);

        if (metadata != null) {
            builder.withTabGroupMetadata(metadata);
        }

        return builder.withTabs(groupedTabs).build();
    }

    /**
     * Start drag by creating a new global state token and invoking the {@link DragAndDropDelegate}.
     *
     * @param dragSourceView {@link View} that initiated drag.
     * @param builder {@link DragShadowBuilder} to build a drag shadow.
     * @param dropData A {@link ChromeDropDataAndroid} object pointing to the data to be transferred
     *     by the drag and drop operation.
     * @return whether the drag started.
     */
    protected boolean startDrag(
            View dragSourceView, DragShadowBuilder builder, ChromeDropDataAndroid dropData) {
        mDragSourceView = dragSourceView;
        sDragToken =
                DragDropGlobalState.store(
                        mMultiInstanceManager.getCurrentInstanceId(), dropData, builder);
        boolean res = mDragAndDropDelegate.startDragAndDrop(dragSourceView, builder, dropData);
        if (!res) {
            // The drag failed to start reset the token.
            clearDragDropGlobalState();
            mDragSourceView = null;
        } else {
            // The drag succeed we can begin the drag.
            setTabDraggingState(dropData, true);
            mDragInProgressSupplier.set(true);
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
        if (dragDropGlobalState != null
                && dragDropGlobalState.getData() instanceof ChromeDropDataAndroid chromeDropData) {
            setTabDraggingState(chromeDropData, false);
        }
        int sourceInstanceId =
                dragDropGlobalState != null
                        ? dragDropGlobalState.getDragSourceInstance()
                        : INVALID_WINDOW_ID;
        boolean isTabGroupDrop = isTabGroupDrop();
        boolean isMultiTabDrop = isMultiTabDrop();

        clearDragDropGlobalState();
        mDragInProgressSupplier.set(false);
        mDragSourceView = null;

        // Close the source instance window if it has no tabs.
        boolean didCloseWindow = mMultiInstanceManager.closeChromeWindowIfEmpty(sourceInstanceId);

        // Only record for source strip to avoid duplicate.
        if (dropHandled) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.SUCCESS, isTabGroupDrop, isMultiTabDrop);
            DragDropMetricUtils.recordDragDropClosedWindow(
                    didCloseWindow, isTabGroupDrop, isMultiTabDrop);
        } else if (MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE)
                >= MultiWindowUtils.getMaxInstances()) {
            mMultiInstanceManager.showInstanceCreationLimitMessage();
            ChromeDragDropUtils.recordTabOrGroupDragToCreateInstanceFailureCount();
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_MAX_INSTANCES, isTabGroupDrop, isMultiTabDrop);
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
        if (sDragToken != null) {
            DragDropGlobalState.clear(sDragToken);
            sDragToken = null;
        }
    }

    /**
     * Retrieves the {@link DragDropGlobalState} for the active drag session.
     *
     * @param dragEvent The current {@link DragEvent}, or null to look up via the stored drag token.
     * @return The active {@link DragDropGlobalState}, or null if no drag state exists.
     */
    @Nullable
    public static DragDropGlobalState getDragDropGlobalState(@Nullable DragEvent dragEvent) {
        if (dragEvent != null) {
            return DragDropGlobalState.getState(dragEvent);
        }
        if (sDragToken != null) {
            return DragDropGlobalState.getState(sDragToken);
        }
        return null;
    }

    /** Currently, don't do anything during Backpress key while tabs being dragged. */
    @Override
    public boolean invokeBackActionOnEscape() {
        return false;
    }

    @Override
    public Boolean handleEscPress() {
        return cancelDrag() == BackPressResult.SUCCESS;
    }

    /** This handler is only active when the tabs are being dragged. */
    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mDragInProgressSupplier;
    }

    protected void onInternalDragStarted() {
        mDragInProgressSupplier.set(true);
    }

    protected void onInternalDragEnded() {
        mDragInProgressSupplier.set(false);
    }

    protected @BackPressResult int cancelDrag() {
        if (mDragSourceView != null) {
            mDragSourceView.cancelDragAndDrop();
            return BackPressResult.SUCCESS;
        }
        return BackPressResult.FAILURE;
    }

    public static void setDragTokenForTesting(Token token) {
        sDragToken = token;
        ResettersForTesting.register(() -> sDragToken = null);
    }

    private static @Nullable List<Tab> getDraggedTabs(ChromeDropDataAndroid dropData) {
        if (dropData instanceof ChromeTabDropDataAndroid tabDropData) {
            return Collections.singletonList(tabDropData.tab);
        } else if (dropData instanceof ChromeMultiTabDropDataAndroid tabsDropData) {
            return tabsDropData.tabs;
        } else if (dropData instanceof ChromeTabGroupDropDataAndroid groupDropData) {
            return groupDropData.tabs;
        }

        assert false : "Unsupported drop data type: " + dropData.getClass().getName();
        return null;
    }

    private void onTabsClosed(List<Tab> closedTabs) {
        // No-op if not currently dragging.
        DragDropGlobalState globalState = getDragDropGlobalState(/* dragEvent= */ null);
        if (globalState == null) return;

        // Check drop data type.
        DropDataAndroid dropData = globalState.getData();
        if (!(dropData instanceof ChromeDropDataAndroid)) return;

        // Find dragged tabs.
        List<Tab> draggedTabs = getDraggedTabs((ChromeDropDataAndroid) dropData);
        if (draggedTabs == null || draggedTabs.isEmpty()) return;

        // If closing a dragged tab, cancel the current drag.
        for (Tab closedTab : closedTabs) {
            for (Tab draggedTab : draggedTabs) {
                if (closedTab.getId() == draggedTab.getId()) {
                    cancelDrag();
                    return;
                }
            }
        }
    }

    private void setTabDraggingState(ChromeDropDataAndroid dropData, boolean isDragging) {
        final List<Tab> tabs = getDraggedTabs(dropData);
        if (tabs == null) return;

        for (Tab tab : tabs) {
            if (tab != null && tab.getUserDataHost() != null && !tab.isDestroyed()) {
                TabDragStateData.getOrCreateForTab(tab).setIsDragging(isDragging);
            }
        }
    }
}
