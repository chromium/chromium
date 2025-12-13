// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ClipDescription;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;

import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/**
 * Define the default behavior when {@link ChromeTabbedActivity} receive drag events that's not
 * consumed by any children views.
 */
@NullMarked
public class ChromeTabbedOnDragListener implements OnDragListener {

    private final MultiInstanceManager mMultiInstanceManager;
    private final TabModelSelector mTabModelSelector;
    private final WindowAndroid mWindowAndroid;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private @Nullable Tab mTabToEnableFakeBox;

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
                Tab selectedTab = mTabModelSelector.getCurrentTab();
                assumeNonNull(selectedTab);
                if (selectedTab.getNativePage() instanceof NewTabPage) {
                    mTabToEnableFakeBox = selectedTab;
                }
                return true;
            case DragEvent.ACTION_DROP:
                // This is to prevent tab switcher from receiving drops. We might support dropping
                // into tab switcher in the future, but this should still be retained to prevent
                // dropping happens on top of tab switcher toolbar.
                boolean isTabGroupDrop =
                        clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
                boolean isMultiTabDrop =
                        clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
                if (mLayoutStateProviderSupplier.get() == null
                        || mLayoutStateProviderSupplier
                                .get()
                                .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                    DragDropMetricUtils.recordDragDropResult(
                            DragDropResult.IGNORED_TAB_SWITCHER,
                            isInDesktopWindow,
                            isTabGroupDrop,
                            isMultiTabDrop);
                    return false;
                }
                if (clipDescription == null) return false;
                if (isTabGroupDrop) {
                    return handleGroupDrop(dragEvent, isInDesktopWindow);
                } else if (isMultiTabDrop) {
                    return handleMultiTabDrop(dragEvent, isInDesktopWindow);
                } else {
                    assert clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB);
                    return handleTabDrop(dragEvent, isInDesktopWindow);
                }
            case DragEvent.ACTION_DRAG_ENDED:
                // Re-enable the NTP we disabled at DRAG_STARTED if any.
                if (mTabToEnableFakeBox != null) {
                    NativePage nativePage = mTabToEnableFakeBox.getNativePage();
                    if (nativePage instanceof NewTabPage ntp) {
                        ntp.enableSearchBoxEditText(true);
                    }
                }

                // If the selected tab changed to a different NTP during the drag, re-enable that
                // too.
                final Tab curSelectedTab = mTabModelSelector.getCurrentTab();
                final NewTabPage currentNtp =
                        (curSelectedTab != null
                                        && curSelectedTab.getNativePage() instanceof NewTabPage ntp)
                                ? ntp
                                : null;

                if (currentNtp != null) {
                    currentNtp.enableSearchBoxEditText(true);
                }
        }
        return false;
    }

    private boolean handleTabDrop(DragEvent dragEvent, boolean isInDesktopWindow) {
        DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
        assertNonNull(globalState);
        Tab draggedTab = ChromeDragDropUtils.getTabFromGlobalState(globalState);
        if (!validDragEvent(
                globalState,
                draggedTab,
                isInDesktopWindow,
                /* isTabGroup= */ false,
                /* isMultiTab= */ false)) {
            return false;
        }

        // Reject cross-model drops if incognito is opened as a new window.
        boolean draggedTabIncognito = draggedTab.isIncognitoBranded();
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && !ChromeDragDropUtils.doesBelongToCurrentModel(
                        draggedTabIncognito, mTabModelSelector)) {
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
                        mWindowAndroid.getActivity().get(), draggedTabIncognito, mTabModelSelector);

        // Reparent the dragged tab to the destination window.
        mMultiInstanceManager.moveTabsToWindow(
                mWindowAndroid.getActivity().get(),
                Collections.singletonList(draggedTab),
                destIndex);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_CONTENT,
                isInDesktopWindow,
                /* isTabGroup= */ false,
                /* isMultiTab= */ false);
        return true;
    }

    private boolean handleMultiTabDrop(DragEvent dragEvent, boolean isInDesktopWindow) {
        DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
        assertNonNull(globalState);
        List<Tab> draggedTabs = ChromeDragDropUtils.getTabsFromGlobalState(globalState);
        if (!validDragEvent(
                globalState,
                draggedTabs,
                isInDesktopWindow,
                /* isTabGroup= */ false,
                /* isMultiTab= */ true)) {
            return false;
        }

        // Reject cross-model drops if incognito is opened as a new window.
        boolean draggedTabsIncognito = draggedTabs.get(0).isIncognitoBranded();
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && !ChromeDragDropUtils.doesBelongToCurrentModel(
                        draggedTabsIncognito, mTabModelSelector)) {
            return false;
        }

        // Determine the destination index for dropping the tabs based on whether the source and
        // destination tab models match.
        final int destIndex =
                ChromeDragDropUtils.handleDropInDifferentModel(
                        mWindowAndroid.getActivity().get(),
                        draggedTabsIncognito,
                        mTabModelSelector);

        // Reparent the dragged tabs to the destination window.
        mMultiInstanceManager.moveTabsToWindow(
                mWindowAndroid.getActivity().get(), draggedTabs, destIndex);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_CONTENT,
                isInDesktopWindow,
                /* isTabGroup= */ false,
                /* isMultiTab= */ true);
        return true;
    }

    private boolean handleGroupDrop(DragEvent dragEvent, boolean isInDesktopWindow) {
        DragDropGlobalState globalState = DragDropGlobalState.getState(dragEvent);
        assertNonNull(globalState);
        TabGroupMetadata tabGroupMetadata =
                ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState);

        if (!validDragEvent(
                globalState,
                tabGroupMetadata,
                isInDesktopWindow,
                /* isTabGroup= */ true,
                /* isMultiTab= */ false)) {
            return false;
        }

        // Reject cross-model drops if incognito is opened as a new window.
        boolean draggedTabGroupIncognito = tabGroupMetadata.isIncognito;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()
                && !ChromeDragDropUtils.doesBelongToCurrentModel(
                        draggedTabGroupIncognito, mTabModelSelector)) {
            return false;
        }

        // Determine the destination index for dropping the tab group based on whether the source
        // and destination tab models match.
        final int destIndex =
                ChromeDragDropUtils.handleDropInDifferentModel(
                        mWindowAndroid.getActivity().get(),
                        draggedTabGroupIncognito,
                        mTabModelSelector);

        // Reparent the dragged tab group to destination window.
        mMultiInstanceManager.moveTabGroupToWindow(
                mWindowAndroid.getActivity().get(), tabGroupMetadata, destIndex);
        DragDropMetricUtils.recordDragDropType(
                DragDropType.TAB_STRIP_TO_CONTENT,
                isInDesktopWindow,
                /* isTabGroup= */ true,
                /* isMultiTab= */ false);
        return true;
    }

    @Contract("null, _, _, _, _ -> false; _, null, _, _, _ -> false")
    private boolean validDragEvent(
            @Nullable DragDropGlobalState globalState,
            @Nullable Object draggedData,
            boolean isInDesktopWindow,
            boolean isTabGroup,
            boolean isMultiTab) {
        if (globalState == null || draggedData == null) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.ERROR_CONTENT_NOT_FOUND,
                    isInDesktopWindow,
                    /* isTabGroup= */ isTabGroup,
                    /* isMultiTab= */ isMultiTab);
            return false;
        }
        if (globalState.isDragSourceInstance(mMultiInstanceManager.getCurrentInstanceId())) {
            DragDropMetricUtils.recordDragDropResult(
                    DragDropResult.IGNORED_SAME_INSTANCE,
                    isInDesktopWindow,
                    /* isTabGroup= */ isTabGroup,
                    /* isMultiTab= */ isMultiTab);
            return false;
        }
        return true;
    }
}
