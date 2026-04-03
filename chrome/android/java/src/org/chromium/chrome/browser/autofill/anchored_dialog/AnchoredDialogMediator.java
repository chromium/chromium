// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.anchored_dialog;

import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayDeque;
import java.util.Queue;
import java.util.function.Supplier;

@NullMarked
class AnchoredDialogMediator {
    private final PropertyModel mModel;
    private final View mContainerView;
    private final Supplier<Integer> mVerticalOffsetProvider;

    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    private final Queue<BottomSheetContent> mContentQueue = new ArrayDeque<>();
    private @StateChangeReason int mHideReason;

    AnchoredDialogMediator(
            PropertyModel model, View containerView, Supplier<Integer> verticalOffsetProvider) {
        mModel = model;
        mContainerView = containerView;
        mVerticalOffsetProvider = verticalOffsetProvider;
    }

    void destroy() {
        mModel.set(AnchoredDialogProperties.VISIBLE, false);
        mObservers.clear();
    }

    boolean requestShowContent(BottomSheetContent content) {
        BottomSheetContent currentContent = mModel.get(AnchoredDialogProperties.CONTENT);
        if (content == currentContent) { // The content is already shown.
            return true;
        }
        if (mContentQueue.contains(content)) { // The content is already queued.
            return false;
        }
        // Add the content to the queue.
        mContentQueue.add(content);

        // If there is no current content, show the content now.
        if (currentContent == null) {
            showNextContent();
            return true;
        }
        return false;
    }

    void hideContent(@Nullable BottomSheetContent content, @StateChangeReason int reason) {
        BottomSheetContent currentContent = mModel.get(AnchoredDialogProperties.CONTENT);
        if (currentContent == null) {
            return;
        }
        // If the specified content is not currently shown, just remove it from the queue.
        if (currentContent != content) {
            mContentQueue.remove(content);
            return;
        }
        // Hide the current content.
        mHideReason = reason;
        mModel.set(AnchoredDialogProperties.VISIBLE, false);
    }

    void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void showNextContent() {
        BottomSheetContent nextContent = mContentQueue.poll();
        if (nextContent == null) {
            return;
        }
        mModel.set(AnchoredDialogProperties.CONTENT, nextContent);
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
            observer.onSheetContentChanged(nextContent);
            observer.onSheetOpened(StateChangeReason.NONE);
            observer.onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        }
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

        // Show the next content in the queue.
        showNextContent();
    }
}
