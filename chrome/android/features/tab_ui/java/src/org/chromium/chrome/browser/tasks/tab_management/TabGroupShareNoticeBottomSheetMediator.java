// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetCoordinator.TabGroupShareNoticeBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for the Shared Tab Group Notice Bottom Sheet. This mediator contains the logic for
 * bottom sheet user interactions.
 */
@NullMarked
public class TabGroupShareNoticeBottomSheetMediator {
    private final BottomSheetController mBottomSheetController;
    private final TabGroupShareNoticeBottomSheetCoordinatorDelegate mDelegate;
    private final PropertyModel mModel;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                    mDelegate.onSheetClosed();
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    onSheetClosed(reason);
                }
            };

    /**
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param delegate For handling view layer interactions.
     */
    @VisibleForTesting
    TabGroupShareNoticeBottomSheetMediator(
            BottomSheetController bottomSheetController,
            TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate) {
        mBottomSheetController = bottomSheetController;
        mDelegate = delegate;

        mModel =
                new PropertyModel.Builder(TabGroupShareNoticeBottomSheetProperties.ALL_KEYS)
                        .with(
                                TabGroupShareNoticeBottomSheetProperties.COMPLETION_HANDLER,
                                () -> hide(StateChangeReason.INTERACTION_COMPLETE))
                        .build();
    }

    /**
     * Requests to show the bottom sheet content. Will not show if the user has already accepted the
     * notice.
     */
    void requestShowContent() {
        if (!mDelegate.requestShowContent()) return;
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    /** Hides the bottom sheet. */
    void hide(@StateChangeReason int hideReason) {
        mDelegate.hide(hideReason);
    }

    /** Returns the model for the bottom sheet. */
    PropertyModel getModel() {
        return mModel;
    }

    @VisibleForTesting
    BottomSheetObserver getBottomSheetObserver() {
        return mBottomSheetObserver;
    }
}
