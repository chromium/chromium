// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

@NullMarked
class AnchoredDialogMediator {
    private final PropertyModel mModel;
    private final View mContainerView;
    private final Supplier<Integer> mVerticalOffsetProvider;

    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    private @StateChangeReason int mHideReason;

    AnchoredDialogMediator(
            PropertyModel model, View containerView, Supplier<Integer> verticalOffsetProvider) {
        mModel = model;
        mContainerView = containerView;
        mVerticalOffsetProvider = verticalOffsetProvider;
    }

    void requestShowContent(BottomSheetContent content) {
        // Hide the dialog if already shown.
        mModel.set(AnchoredDialogProperties.VISIBLE, false);

        mModel.set(AnchoredDialogProperties.CONTENT, content);
        mModel.set(AnchoredDialogProperties.CONTAINER_VIEW, mContainerView);
        mModel.set(AnchoredDialogProperties.ON_DISMISS_LISTENER, this::onDismiss);
        mHideReason = StateChangeReason.NONE;

        // Calculate the location of the container view in the window.
        int[] containerLocation = new int[2];
        mContainerView.getLocationInWindow(containerLocation);
        // Calculate the offset from the top of the window.
        int offsetY = mVerticalOffsetProvider.get() + containerLocation[1];

        mModel.set(AnchoredDialogProperties.OFFSET_Y, offsetY);
        mModel.set(AnchoredDialogProperties.VISIBLE, true);

        for (BottomSheetObserver observer : mObservers) {
            observer.onSheetContentChanged(content);
            observer.onSheetOpened(StateChangeReason.NONE);
            observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        }
    }

    void hideContent(@Nullable BottomSheetContent content, @StateChangeReason int reason) {
        if (mModel.get(AnchoredDialogProperties.CONTENT) != content) {
            return;
        }
        mHideReason = reason;
        mModel.set(AnchoredDialogProperties.VISIBLE, false);
    }

    void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void onDismiss() {
        mModel.set(AnchoredDialogProperties.VISIBLE, false);

        mModel.get(AnchoredDialogProperties.CONTENT).destroy();
        mModel.set(AnchoredDialogProperties.CONTENT, null);

        for (BottomSheetObserver observer : mObservers) {
            // Call observer methods in the same order as BottomSheetCoordinator.
            observer.onSheetClosed(mHideReason);
            observer.onSheetStateChanged(SheetState.HIDDEN, mHideReason);
            observer.onSheetContentChanged(null);
        }
    }
}
