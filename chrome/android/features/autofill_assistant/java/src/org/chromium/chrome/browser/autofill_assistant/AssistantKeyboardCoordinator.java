// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.widget.TextView;

import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;

/**
 * Coordinator responsible for enabling or disabling the soft keyboard.
 */
class AssistantKeyboardCoordinator {
    private final Activity mActivity;
    private final ActivityKeyboardVisibilityDelegate mKeyboardDelegate;
    private final CompositorViewHolder mCompositorViewHolder;
    private final KeyboardVisibilityListener mKeyboardVisibilityListener =
            this::onKeyboardVisibilityChanged;
    private boolean mAllowShowingSoftKeyboard = true;
    private Delegate mDelegate;
    private final BottomSheetController mBottomSheetController;

    interface Delegate {
        void onKeyboardVisibilityChanged(boolean visible);
    }

    // TODO(b/173103628): refactor and inject the keyboard delegate directly.
    AssistantKeyboardCoordinator(Activity activity,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            CompositorViewHolder compositorViewHolder, AssistantModel model, Delegate delegate,
            BottomSheetController controller) {
        mActivity = activity;
        mKeyboardDelegate = keyboardVisibilityDelegate;
        mCompositorViewHolder = compositorViewHolder;
        mDelegate = delegate;
        mBottomSheetController = controller;

        model.addObserver((source, propertyKey) -> {
            if (AssistantModel.VISIBLE == propertyKey) {
                if (model.get(AssistantModel.VISIBLE)) {
                    enableListenForKeyboardVisibility(true);
                } else {
                    enableListenForKeyboardVisibility(false);
                }
            } else if (AssistantModel.ALLOW_SOFT_KEYBOARD == propertyKey) {
                allowShowingSoftKeyboard(model.get(AssistantModel.ALLOW_SOFT_KEYBOARD));
            }
        });
    }

    /** Returns whether the keyboard is currently shown. */
    boolean isKeyboardShown() {
        return mKeyboardDelegate.isKeyboardShowing(mActivity, mCompositorViewHolder);
    }

    /** Returns whether the BottomSheet is currently shown. */
    private boolean isBottomSheetShown() {
        return mBottomSheetController.getSheetState() != SheetState.HIDDEN;
    }

    /** Hides the keyboard. */
    void hideKeyboard() {
        mKeyboardDelegate.hideKeyboard(mCompositorViewHolder);
    }

    /** Hides the keyboard after a delay if the focus is not on a TextView */
    void hideKeyboardIfFocusNotOnText() {
        if (!(mActivity.getCurrentFocus() instanceof TextView)) {
            hideKeyboard();
        }
    }

    /** Start or stop listening for keyboard visibility changes. */
    private void enableListenForKeyboardVisibility(boolean enabled) {
        if (enabled) {
            mKeyboardDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        } else {
            mKeyboardDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        }
    }

    /** Set soft keyboard allowed state. */
    private void allowShowingSoftKeyboard(boolean allowed) {
        mAllowShowingSoftKeyboard = allowed;
        if (!allowed && isBottomSheetShown()) {
            hideKeyboard();
        }
    }

    /** If the keyboard shows up and is not allowed, hide it. */
    private void onKeyboardVisibilityChanged(boolean isShowing) {
        mDelegate.onKeyboardVisibilityChanged(isShowing);
        if (isShowing && !mAllowShowingSoftKeyboard && isBottomSheetShown()) {
            hideKeyboard();
        }
    }
}
