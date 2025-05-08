// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.ClipDescription;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;

/**
 * Define the default behavior when {@link ChromeTabbedActivity} receive drag events that's not
 * consumed by any children views.
 */
public class ChromeTabbedOnDragListener implements OnDragListener {

    private final MultiInstanceManager mMultiInstanceManager;
    private final TabModelSelector mTabModelSelector;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final DesktopWindowStateManager mDesktopWindowStateManager;

    /**
     * Drag and Drop listener defines the default behavior {@link ChromeTabbedActivity} receive drag
     * events that's not consumed by any children views.
     *
     * @param multiInstanceManager The current {@link MultiInstanceManager}.
     * @param tabModelSelector Contains tab model info {@link TabModelSelector}.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} to determine desktop
     *     windowing mode state.
     */
    public ChromeTabbedOnDragListener(
            MultiInstanceManager multiInstanceManager,
            TabModelSelector tabModelSelector,
            WindowAndroid windowAndroid,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        mMultiInstanceManager = multiInstanceManager;
        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mDesktopWindowStateManager = desktopWindowStateManager;
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        boolean isInDesktopWindow = AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager);
        ClipDescription clipDescription = dragEvent.getClipDescription();
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                // Only proceed if browser content is being dragged; otherwise, skip the operations.
                if (!MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)
                        || !DragDropGlobalState.hasValue()) {
                    return false;
                }
                return true;
            case DragEvent.ACTION_DROP:
                // This is to prevent tab switcher from receiving drops. We might support dropping
                // into tab switcher in the future, but this should still be retained to prevent
                // dropping happens on top of tab switcher toolbar.
                boolean isTabGroupDrop =
                        clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
                if (mLayoutStateProviderSupplier.get() == null
                        || mLayoutStateProviderSupplier
                                .get()
                                .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                    DragDropMetricUtils.recordDragDropResult(
                            DragDropResult.IGNORED_TAB_SWITCHER, isInDesktopWindow, isTabGroupDrop);
                    return false;
                }
                if (clipDescription == null) return false;
                if (isTabGroupDrop) {
                    return handleGroupDrop(dragEvent, isInDesktopWindow);
                } else {
                    assert clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB);
                    return handleTabDrop(dragEvent, isInDesktopWindow);
                }
        }
        return false;
    }

    private boolean handleTabDrop(DragEvent dragEvent, boolean isInDesktopWindow) {
        DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
        Tab draggedTab = ChromeDragDropUtils.getTabFromGlobalState(globalState);
        if (globalState == null || draggedTab == null) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.ERROR_CONTENT_NOT_FOUND,
                    isInDesktopWindow,
                    /* isTabGroup= */ false);
            return false;
        }
        if (globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId())) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_SAME_INSTANCE,
                    isInDesktopWindow,
                    /* isTabGroup= */ false);
            return false;
        }

        // Record user action if a grouped tab is going to be re-parented.
        if (ChromeDragDropUtils.isTabInGroupFromGlobalState(globalState)) {
            RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
        }

        // Determine the destination index for dropping the tab based on whether the source and
        // destination tab models match.
        final int destIndex =
                ChromeDragDropUtils.handleDropInDifferentModel(
                        mWindowAndroid.getActivity().get(),
                        draggedTab.isIncognitoBranded(),
                        mTabModelSelector);

        // Reparent the dragged tab to the destination window.
        mMultiInstanceManager.moveTabToWindow(
                mWindowAndroid.getActivity().get(), draggedTab, destIndex);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_CONTENT, isInDesktopWindow, /* isTabGroup= */ false);
        return true;
    }

    private boolean handleGroupDrop(DragEvent dragEvent, boolean isInDesktopWindow) {
        DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
        TabGroupMetadata tabGroupMetadata =
                ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState);
        if (globalState == null || tabGroupMetadata == null) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.ERROR_CONTENT_NOT_FOUND,
                    isInDesktopWindow,
                    /* isTabGroup= */ true);
            return false;
        }
        if (globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId())) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_SAME_INSTANCE,
                    isInDesktopWindow,
                    /* isTabGroup= */ true);
            return false;
        }

        // Determine the destination index for dropping the tab group based on whether the source
        // and destination tab models match.
        final int destIndex =
                ChromeDragDropUtils.handleDropInDifferentModel(
                        mWindowAndroid.getActivity().get(),
                        tabGroupMetadata.isIncognito,
                        mTabModelSelector);

        // Reparent the dragged tab group to destination window.
        mMultiInstanceManager.moveTabGroupToWindow(
                mWindowAndroid.getActivity().get(), tabGroupMetadata, destIndex);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_CONTENT, isInDesktopWindow, /* isTabGroup= */ true);
        return true;
    }
}
