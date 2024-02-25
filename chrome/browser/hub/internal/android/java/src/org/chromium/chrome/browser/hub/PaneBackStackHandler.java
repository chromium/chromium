// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

import java.util.LinkedList;

/**
 * Manages back navigations between Panes.
 *
 * <p>The back navigation stack is a stack containing visited {@link Pane}s in most recently visited
 * to least recently visited. The current {@link Pane} is not in the stack. When a pane is focused
 * by any action other than back press, the previous {@link Pane} is added to the stack and the
 * current {@link Pane} is removed from the stack. When a back press happens, the most recent (top
 * of the stack) {@link Pane} is focused, but the previous {@link Pane} is not re-added to the stack
 * to prevent an infinite loop.
 */
public class PaneBackStackHandler implements BackPressHandler {
    private final @NonNull PaneManager mPaneManager;
    private final @NonNull ObservableSupplierImpl<Boolean> mHandleBackPressSupplier;
    private final @NonNull LinkedList<Pane> mBackStack;
    private final @NonNull Callback<Pane> mOnPaneFocusedCallback;
    private @Nullable Pane mCurrentPane;

    /**
     * Handler for back navigations between Panes.
     *
     * @param paneManager The {@link PaneManager} of the Hub.
     */
    public PaneBackStackHandler(@NonNull PaneManager paneManager) {
        mPaneManager = paneManager;
        mHandleBackPressSupplier = new ObservableSupplierImpl<>();
        mHandleBackPressSupplier.set(false);

        mBackStack = new LinkedList<>();

        mOnPaneFocusedCallback = this::onPaneFocused;
        paneManager.getFocusedPaneSupplier().addObserver(mOnPaneFocusedCallback);
    }

    /** Destroys the object cleaning up observers and the stack. */
    public void destroy() {
        reset();
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnPaneFocusedCallback);
    }

    /**
     * Resets the back stack to include no entries. Use when leaving the Hub and a full teardown is
     * not performed.
     */
    public void reset() {
        mHandleBackPressSupplier.set(false);
        mBackStack.clear();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        assert mHandleBackPressSupplier.get()
                : "Handling back press when not accepting back presses.";
        assert !mBackStack.isEmpty()
                : "mBackStack should not be empty if handleBackPress is valid.";

        while (!mBackStack.isEmpty()) {
            // Set mCurrentPane to null so it isn't re-added to mBackStack.
            mCurrentPane = null;
            Pane nextPane = mBackStack.removeFirst();

            // In practice failing to focus and falling back should be rare or impossible; however,
            // we handle the condition to ensure back handling doesn't break if this were to ever
            // become commonplace.
            if (!mPaneManager.focusPane(nextPane.getPaneId())) continue;

            if (mBackStack.isEmpty()) {
                mHandleBackPressSupplier.set(false);
            }
            return BackPressResult.SUCCESS;
        }

        // mBackStack was emptied without navigating.
        mHandleBackPressSupplier.set(false);
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressSupplier;
    }

    private void onPaneFocused(Pane pane) {
        // `pane` is the newly focused pane. At this point mCurrentPane is the previous pane.
        if (mCurrentPane != null && mCurrentPane.getReferenceButtonDataSupplier().hasValue()) {
            mBackStack.addFirst(mCurrentPane);
            mHandleBackPressSupplier.set(true);
        }
        mCurrentPane = pane;
        mBackStack.remove(pane);
    }
}
