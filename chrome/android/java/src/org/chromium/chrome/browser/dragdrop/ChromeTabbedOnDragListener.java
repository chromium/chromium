// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.WindowAndroid;

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
        Tab draggedTab = DragDropGlobalState.getInstance().tabBeingDragged;
        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                // Only proceed with the dragged tab; otherwise, skip the operations.
                if (dragEvent.getClipDescription().filterMimeTypes(
                                 ChromeDragAndDropBrowserDelegate.CHROME_MIMETYPE_TAB) == null
                        || draggedTab == null) {
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
                    return false;
                }
                if (!isSourceInstance()) {
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
                    return true;
                }
                return false;
            case DragEvent.ACTION_DRAG_ENDED:
                // Clear drag drop global state.
                DragDropGlobalState.getInstance().reset();
                return true;
        }
        return false;
    }

    private boolean isSourceInstance() {
        return DragDropGlobalState.getInstance().dragSourceInstanceId
                == mMultiInstanceManager.getCurrentInstanceId();
    }
}
