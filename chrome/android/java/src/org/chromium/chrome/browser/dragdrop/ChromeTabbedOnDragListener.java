// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropTabResult;
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

    /**
     * Drag and Drop listener defines the default behavior {@link ChromeTabbedActivity} receive drag
     * events that's not consumed by any children views.
     *
     * @param multiInstanceManager The current {@link MultiInstanceManager}.
     * @param tabModelSelector Contains tab model info {@link TabModelSelector}.
     * @param windowAndroid The current {@link WindowAndroid}.
     */
    public ChromeTabbedOnDragListener(
            MultiInstanceManager multiInstanceManager,
            TabModelSelector tabModelSelector,
            WindowAndroid windowAndroid,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mMultiInstanceManager = multiInstanceManager;
        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
    }

    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                // Only proceed with the dragged tab; otherwise, skip the operations.
                if (!DragDropGlobalState.hasValue()
                        || dragEvent
                                        .getClipDescription()
                                        .filterMimeTypes(MimeTypeUtils.CHROME_MIMETYPE_TAB)
                                == null) {
                    return false;
                }
                return true;
            case DragEvent.ACTION_DROP:
                // This is to prevent tab switcher from receiving drops. We might support dropping
                // into tab switcher in the future, but this should still be retained to prevent
                // dropping happens on top of tab switcher toolbar.
                if (mLayoutStateProviderSupplier.get() == null
                        || mLayoutStateProviderSupplier
                                .get()
                                .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                    DragDropMetricUtils.recordTabDragDropResult(
                            DragDropTabResult.IGNORED_TAB_SWITCHER);
                    return false;
                }

                DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
                Tab draggedTab = getTabFromGlobalState(globalState);
                if (globalState == null || draggedTab == null) {
                    DragDropMetricUtils.recordTabDragDropResult(
                            DragDropTabResult.ERROR_TAB_NOT_FOUND);
                    return false;
                }
                if (globalState.isDragSourceInstance(
                        mMultiInstanceManager.getCurrentInstanceId())) {
                    DragDropMetricUtils.recordTabDragDropResult(
                            DragDropTabResult.IGNORED_SAME_INSTANCE);
                    return false;
                }

                // Record user action if a grouped tab is going to be re-parented.
                if (isTabInGroupFromGlobalState(globalState)) {
                    RecordUserAction.record("MobileToolbarReorderTab.TabRemovedFromGroup");
                }

                // Reparent the dragged tab to the position immediately following the selected
                // tab in the destination window.
                Tab currentTab = mTabModelSelector.getCurrentTab();
                mMultiInstanceManager.moveTabToWindow(
                        mWindowAndroid.getActivity().get(),
                        draggedTab,
                        TabModelUtils.getTabIndexById(
                                        mTabModelSelector.getModel(currentTab.isIncognito()),
                                        currentTab.getId())
                                + 1);
                DragDropMetricUtils.recordTabDragDropType(DragDropType.TAB_STRIP_TO_CONTENT);
                return true;
        }
        return false;
    }

    private Tab getTabFromGlobalState(@NonNull DragDropGlobalState globalState) {
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        if (globalState.getData() instanceof ChromeDropDataAndroid) {
            return ((ChromeDropDataAndroid) globalState.getData()).tab;
        } else {
            return null;
        }
    }

    private boolean isTabInGroupFromGlobalState(@NonNull DragDropGlobalState globalState) {
        // We should only attempt to access this while we know there's an active drag.
        assert globalState != null : "Attempting to access dragged tab with invalid drag state.";
        if (globalState.getData() instanceof ChromeDropDataAndroid) {
            return ((ChromeDropDataAndroid) globalState.getData()).isTabInGroup;
        } else {
            return false;
        }
    }
}
