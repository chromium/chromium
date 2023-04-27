// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fold_transitions;

import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * A utility class to handle saving and restoring the UI state across fold transitions.
 */
public class FoldTransitionController {
    @VisibleForTesting
    public static final String DID_CHANGE_TABLET_MODE = "did_change_tablet_mode";
    public static final long KEYBOARD_RESTORATION_TIMEOUT_MS = 2 * 1000; // 2 seconds
    static final String URL_BAR_FOCUS_STATE = "url_bar_focus_state";
    static final String URL_BAR_EDIT_TEXT = "url_bar_edit_text";
    static final String KEYBOARD_VISIBILITY_STATE = "keyboard_visibility_state";

    /**
     * Saves the relevant UI when the activity is recreated on a device fold transition. Expected to
     * be invoked during {@code Activity#onSaveInstanceState()}.
     *
     * @param savedInstanceState The {@link Bundle} where the UI state will be saved.
     * @param toolbarManager The {@link ToolbarManager} for the current activity.
     * @param didChangeTabletMode Whether the activity is recreated due to a fold configuration
     *         change. {@code true} if the fold configuration changed, {@code false} otherwise.
     * @param actualKeyboardVisibilityState Whether the soft keyboard is visible, {@code true} if it
     *         is, {@code false} otherwise.
     */
    public static void saveUiState(Bundle savedInstanceState, ToolbarManager toolbarManager,
            boolean didChangeTabletMode, boolean actualKeyboardVisibilityState) {
        savedInstanceState.putBoolean(DID_CHANGE_TABLET_MODE, didChangeTabletMode);
        saveOmniboxState(savedInstanceState, toolbarManager);
        saveKeyboardState(savedInstanceState, actualKeyboardVisibilityState);
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     * @param toolbarManager The {@link ToolbarManager} for the current activity.
     * @param layoutManager The {@link LayoutStateProvider} for the current activity.
     * @param layoutStateHandler The {@link Handler} to post UI state restoration.
     * @param activityTabProvider The current activity tab provider.
     */
    public static void restoreUiState(Bundle savedInstanceState, ToolbarManager toolbarManager,
            LayoutStateProvider layoutManager, Handler layoutStateHandler,
            ActivityTabProvider activityTabProvider) {
        if (savedInstanceState == null || layoutManager == null) {
            return;
        }

        // Restore the UI state only on a device fold transition.
        if (!savedInstanceState.getBoolean(DID_CHANGE_TABLET_MODE, false)) {
            return;
        }

        restoreOmniboxState(savedInstanceState, toolbarManager, layoutManager, layoutStateHandler);
        restoreKeyboardState(
                savedInstanceState, activityTabProvider, layoutManager, layoutStateHandler);
    }

    /**
     * Determines whether the keyboard state should be saved during a fold transition. The keyboard
     * state will be saved only if the web contents has a focused editable node.
     *
     * @param activityTabProvider The current activity tab provider.
     * @return {@code true} if the keyboard state should be saved, {@code false} otherwise.
     */
    public static boolean shouldSaveKeyboardState(ActivityTabProvider activityTabProvider) {
        if (activityTabProvider.get() == null
                || activityTabProvider.get().getWebContents() == null) {
            return false;
        }
        return activityTabProvider.get().getWebContents().isFocusedElementEditable();
    }

    /**
     * Determines whether the soft keyboard is visible.
     *
     * @param activityTabProvider The current activity tab provider.
     * @return {@code true} if the keyboard is visible, false otherwise.
     */
    public static boolean isKeyboardVisible(@NonNull ActivityTabProvider activityTabProvider) {
        if (activityTabProvider.get() == null || activityTabProvider.get().getWebContents() == null
                || activityTabProvider.get().getWebContents().getViewAndroidDelegate() == null) {
            return false;
        }

        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                activityTabProvider.get().getContext(),
                activityTabProvider.get()
                        .getWebContents()
                        .getViewAndroidDelegate()
                        .getContainerView());
    }

    /**
     * Determines whether the keyboard visibility state is valid for restoration after a fold
     * transition.
     *
     * @param timestamp The time (in milliseconds) at which the keyboard visibility state was saved
     *         during a fold transition.
     * @return {@code true} if the keyboard visibility state is valid for restoration, {@code false}
     *         otherwise.
     */
    public static boolean isKeyboardStateValid(Long timestamp) {
        return timestamp != null
                && SystemClock.elapsedRealtime() - timestamp <= KEYBOARD_RESTORATION_TIMEOUT_MS;
    }

    private static void restoreUiStateOnLayoutDoneShowing(LayoutStateProvider layoutManager,
            Handler layoutStateHandler, Runnable onLayoutFinishedShowing) {
        /* TODO (crbug/1395495): Restore the UI state directly if the invocation of {@code
         * StaticLayout#requestFocus(Tab)} in {@code StaticLayout#doneShowing()} is removed. We
         * should restore the desired UI state after the {@link StaticLayout} is done showing to
         * persist the state. If the layout is visible and done showing, it is safe to execute the
         * UI restoration runnable directly to persist the desired UI state. */
        if (layoutManager.isLayoutVisible(LayoutType.BROWSING)
                && !layoutManager.isLayoutStartingToShow(LayoutType.BROWSING)) {
            onLayoutFinishedShowing.run();
        } else {
            layoutManager.addObserver(new LayoutStateObserver() {
                @Override
                public void onFinishedShowing(int layoutType) {
                    assert layoutManager.isLayoutVisible(LayoutType.BROWSING)
                        : "LayoutType is "
                            + layoutManager.getActiveLayoutType()
                            + ", expected BROWSING type on activity start.";
                    LayoutStateObserver.super.onFinishedShowing(layoutType);
                    layoutStateHandler.post(() -> {
                        onLayoutFinishedShowing.run();
                        layoutManager.removeObserver(this);
                        layoutStateHandler.removeCallbacksAndMessages(null);
                    });
                }
            });
        }
    }

    private static void saveOmniboxState(Bundle savedInstanceState, ToolbarManager toolbarManager) {
        if (savedInstanceState == null || toolbarManager == null) {
            return;
        }
        if (toolbarManager.isUrlBarFocused()) {
            savedInstanceState.putBoolean(URL_BAR_FOCUS_STATE, true);
            savedInstanceState.putString(
                    URL_BAR_EDIT_TEXT, toolbarManager.getUrlBarTextWithoutAutocomplete());
        }
    }

    private static void saveKeyboardState(
            Bundle savedInstanceState, boolean keyboardVisibilityState) {
        if (savedInstanceState == null || !keyboardVisibilityState) {
            return;
        }
        savedInstanceState.putBoolean(KEYBOARD_VISIBILITY_STATE, true);
    }

    private static void restoreOmniboxState(@NonNull Bundle savedInstanceState,
            ToolbarManager toolbarManager, @NonNull LayoutStateProvider layoutManager,
            Handler layoutStateHandler) {
        if (toolbarManager == null) {
            return;
        }
        if (!savedInstanceState.getBoolean(URL_BAR_FOCUS_STATE, false)) {
            return;
        }
        String urlBarText = savedInstanceState.getString(URL_BAR_EDIT_TEXT, "");
        restoreUiStateOnLayoutDoneShowing(layoutManager, layoutStateHandler,
                () -> setUrlBarFocusAndText(toolbarManager, urlBarText));
    }

    private static void restoreKeyboardState(@NonNull Bundle savedInstanceState,
            ActivityTabProvider activityTabProvider, @NonNull LayoutStateProvider layoutManager,
            Handler layoutStateHandler) {
        if (activityTabProvider == null) {
            return;
        }
        // Restore the keyboard only if the omnibox focus was not restored, because omnibox code
        // is assumed to restore the keyboard on omnibox focus restoration.
        if (savedInstanceState.getBoolean(URL_BAR_FOCUS_STATE, false)) {
            return;
        }
        if (!savedInstanceState.getBoolean(KEYBOARD_VISIBILITY_STATE, false)) {
            return;
        }
        restoreUiStateOnLayoutDoneShowing(
                layoutManager, layoutStateHandler, () -> showSoftInput(activityTabProvider));
    }

    private static void setUrlBarFocusAndText(ToolbarManager toolbarManager, String urlBarText) {
        toolbarManager.setUrlBarFocusAndText(
                true, OmniboxFocusReason.FOLD_TRANSITION_RESTORATION, urlBarText);
    }

    private static void showSoftInput(@NonNull ActivityTabProvider activityTabProvider) {
        var tab = activityTabProvider.get();
        if (tab == null) {
            return;
        }
        var webContents = tab.getWebContents();
        if (webContents == null || webContents.getViewAndroidDelegate() == null) {
            return;
        }

        var containerView = webContents.getViewAndroidDelegate().getContainerView();
        webContents.scrollFocusedEditableNodeIntoView();
        KeyboardVisibilityDelegate.getInstance().showKeyboard(containerView);
    }
}
