// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.task.PostTask;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Utility class to facilitate showing instances of {@code AssistantBottomSheetContent} in the
 * Chrome bottom sheet.
 */
public class BottomSheetUtils {
    /** Request {@code controller} to show {@code content} and expand the sheet when it is shown. */
    public static void showContentAndMaybeExpand(BottomSheetController controller,
            AssistantBottomSheetContent content, boolean shouldExpand, boolean animate) {
        // Show the content.
        boolean contentShown = controller.requestShowContent(content, animate);
        if (!shouldExpand) {
            return;
        }
        if (contentShown) {
            controller.expandSheet();
        } else {
            // If the content is not directly shown, add an observer that will expand the sheet
            // when it is.
            controller.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent == content) {
                        controller.removeObserver(this);
                        PostTask.postTask(
                                UiThreadTaskTraits.DEFAULT, () -> controller.expandSheet());
                    }
                }
            });
        }
    }

    static void restoreState(BottomSheetController controller, BottomSheetContent content,
            @SheetState int targetState) {
        if (controller.getSheetState() == targetState) {
            return;
        }
        if (controller.getCurrentSheetContent() == content) {
            restoreStateInternal(controller, content, targetState);
        } else {
            controller.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetContentChanged(BottomSheetContent newContent) {
                    if (newContent == content) {
                        controller.removeObserver(this);
                        restoreStateInternal(controller, content, targetState);
                    }
                }
            });
        }
    }

    private static void restoreStateInternal(BottomSheetController controller,
            BottomSheetContent content, @SheetState int targetState) {
        if (controller.getSheetState() != SheetState.SCROLLING) {
            setStateInternal(controller, content, targetState);
        } else {
            controller.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(int newState) {
                    controller.removeObserver(this);
                    if (newState != targetState) {
                        setStateInternal(controller, content, targetState);
                    }
                }
            });
        }
    }

    private static void setStateInternal(BottomSheetController controller,
            BottomSheetContent content, @SheetState int targetState) {
        if (targetState == SheetState.HIDDEN && !controller.isSheetHiding()) {
            controller.hideContent(content, /* animate = */ false);
        } else if (targetState == SheetState.PEEK) {
            controller.collapseSheet(/* animate = */ false);
        } else if (targetState == SheetState.HALF || targetState == SheetState.FULL) {
            controller.expandSheet();
        }
    }

    private BottomSheetUtils() {}
}
