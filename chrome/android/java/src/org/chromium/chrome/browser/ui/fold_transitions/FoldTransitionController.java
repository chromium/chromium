// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fold_transitions;

import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** A utility class to handle saving and restoring the UI state across fold transitions. */
public class FoldTransitionController {
    public static final String DID_CHANGE_TABLET_MODE = "did_change_tablet_mode";
    public static final long KEYBOARD_RESTORATION_TIMEOUT_MS = 2 * 1000; // 2 seconds
    static final String URL_BAR_FOCUS_STATE = "url_bar_focus_state";
    static final String URL_BAR_EDIT_TEXT = "url_bar_edit_text";
    static final String KEYBOARD_VISIBILITY_STATE = "keyboard_visibility_state";
    static final String TAB_SWITCHER_VISIBILITY_STATE = "tab_switcher_visibility_state";

    private final OneshotSupplier<ToolbarManager> mToolbarManagerSupplier;
    private final ObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final ActivityTabProvider mActivityTabProvider;
    private final Handler mLayoutStateHandler;
    private boolean mKeyboardVisibleDuringFoldTransition;
    private Long mKeyboardVisibilityTimestamp;

    /**
     * Construct a {@link FoldTransitionController} instance.
     *
     * @param toolbarManagerSupplier The {@link ToolbarManager} instance supplier.
     * @param layoutManagerSupplier The {@link LayoutManager} instance supplier.
     * @param activityTabProvider The current activity tab provider.
     * @param layoutStateHandler The {@link Handler} to post UI state restoration.
     */
    public FoldTransitionController(
            @NonNull OneshotSupplierImpl<ToolbarManager> toolbarManagerSupplier,
            @NonNull ObservableSupplier<LayoutManager> layoutManagerSupplier,
            @NonNull ActivityTabProvider activityTabProvider,
            Handler layoutStateHandler) {
        mToolbarManagerSupplier = toolbarManagerSupplier;
        mLayoutManagerSupplier = layoutManagerSupplier;
        mActivityTabProvider = activityTabProvider;
        mLayoutStateHandler = layoutStateHandler;
    }

    /**
     * Saves the relevant UI when the activity is recreated on a device fold transition. Expected to
     * be invoked during {@code Activity#onSaveInstanceState()}.
     *
     * @param savedInstanceState The {@link Bundle} where the UI state will be saved.
     * @param didChangeTabletMode Whether the activity is recreated due to a fold configuration
     *         change. {@code true} if the fold configuration changed, {@code false} otherwise.
     * @param isIncognito Whether the current TabModel is incognito mode.
     */
    public void saveUiState(
            Bundle savedInstanceState, boolean didChangeTabletMode, boolean isIncognito) {
        if (savedInstanceState == null) return;

        savedInstanceState.putBoolean(DID_CHANGE_TABLET_MODE, didChangeTabletMode);
        if (mToolbarManagerSupplier.hasValue() && mToolbarManagerSupplier.get().isUrlBarFocused()) {
            savedInstanceState.putBoolean(URL_BAR_FOCUS_STATE, true);
            savedInstanceState.putString(
                    URL_BAR_EDIT_TEXT,
                    mToolbarManagerSupplier.get().getUrlBarTextWithoutAutocomplete());
        }

        if (getKeyboardVisibilityState()) {
            savedInstanceState.putBoolean(KEYBOARD_VISIBILITY_STATE, true);
        }

        if (mLayoutManagerSupplier.hasValue()) {
            if (mLayoutManagerSupplier.get().isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                savedInstanceState.putBoolean(TAB_SWITCHER_VISIBILITY_STATE, true);
            }
        }
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     */
    public void restoreUiState(Bundle savedInstanceState) {
        if (savedInstanceState == null || !mLayoutManagerSupplier.hasValue()) {
            return;
        }

        // Restore the UI state only on a device fold transition.
        if (!savedInstanceState.getBoolean(DID_CHANGE_TABLET_MODE, false)) {
            return;
        }

        restoreOmniboxState(
                savedInstanceState,
                mToolbarManagerSupplier.get(),
                mLayoutManagerSupplier.get(),
                mLayoutStateHandler);
        restoreKeyboardState(
                savedInstanceState,
                mActivityTabProvider,
                mLayoutManagerSupplier.get(),
                mLayoutStateHandler);
        restoreTabSwitcherState(savedInstanceState, mLayoutManagerSupplier.get());
    }

    boolean getKeyboardVisibleDuringFoldTransitionForTesting() {
        return mKeyboardVisibleDuringFoldTransition;
    }

    Long getKeyboardVisibilityTimestampForTesting() {
        return mKeyboardVisibilityTimestamp;
    }

    private boolean getKeyboardVisibilityState() {
        if (!shouldSaveKeyboardState(mActivityTabProvider)) {
            return false;
        }

        var actualKeyboardVisibilityState = false;
        var keyboardVisible = isKeyboardVisible(mActivityTabProvider);
        if (keyboardVisible) {
            // The keyboard is currently visible.
            actualKeyboardVisibilityState = true;
            mKeyboardVisibleDuringFoldTransition = true;
            mKeyboardVisibilityTimestamp = SystemClock.elapsedRealtime();
        } else if (mKeyboardVisibleDuringFoldTransition) {
            // This is to handle the case when folding a device invokes Activity#onStop twice
            // (see crbug.com/1426678 for details), thereby invoking #onSaveInstanceState twice.
            // In this flow, Activity#onPause is also invoked twice, and the first call to
            // #onPause hides the keyboard if it is visible, while also clearing the previous
            // instance state. The actual keyboard visibility state during the second invocation
            // is determined by |mKeyboardVisibleDuringFoldTransition| that will be used only if
            // it is valid in terms of a timeout within which the fold transition occurs, to
            // avoid erroneously setting the keyboard state under other circumstances if
            // |mKeyboardVisibleDuringFoldTransition| is not reset.
            if (isKeyboardStateValid(mKeyboardVisibilityTimestamp)) {
                actualKeyboardVisibilityState = true;
            }
            mKeyboardVisibleDuringFoldTransition = false;
            mKeyboardVisibilityTimestamp = null;
        }
        return actualKeyboardVisibilityState;
    }

    /**
     * Determines whether the keyboard state should be saved during a fold transition. The keyboard
     * state will be saved only if the web contents has a focused editable node.
     *
     * @param activityTabProvider The current activity tab provider.
     * @return {@code true} if the keyboard state should be saved, {@code false} otherwise.
     */
    private static boolean shouldSaveKeyboardState(ActivityTabProvider activityTabProvider) {
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
     * @return {@code true} if the keyboard is visible, {@code false} otherwise.
     */
    private static boolean isKeyboardVisible(@NonNull ActivityTabProvider activityTabProvider) {
        if (activityTabProvider.get() == null
                || activityTabProvider.get().getWebContents() == null
                || activityTabProvider.get().getWebContents().getViewAndroidDelegate() == null) {
            return false;
        }

        return KeyboardVisibilityDelegate.getInstance()
                .isKeyboardShowing(
                        activityTabProvider.get().getContext(),
                        activityTabProvider
                                .get()
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
    private static boolean isKeyboardStateValid(Long timestamp) {
        return timestamp != null
                && SystemClock.elapsedRealtime() - timestamp <= KEYBOARD_RESTORATION_TIMEOUT_MS;
    }

    private static void restoreUiStateOnLayoutDoneShowing(
            LayoutManager layoutManager,
            Handler layoutStateHandler,
            Runnable onLayoutFinishedShowing) {
        /* TODO (crbug/1395495): Restore the UI state directly if the invocation of {@code
         * StaticLayout#requestFocus(Tab)} in {@code StaticLayout#doneShowing()} is removed. We
         * should restore the desired UI state after the {@link StaticLayout} is done showing to
         * persist the state. If the layout is visible and done showing, it is safe to execute the
         * UI restoration runnable directly to persist the desired UI state. */
        if (layoutManager.isLayoutVisible(LayoutType.BROWSING)
                && !layoutManager.isLayoutStartingToShow(LayoutType.BROWSING)) {
            onLayoutFinishedShowing.run();
        } else {
            layoutManager.addObserver(
                    new LayoutStateObserver() {
                        @Override
                        public void onFinishedShowing(int layoutType) {
                            assert layoutManager.isLayoutVisible(LayoutType.BROWSING)
                                    : "LayoutType is "
                                            + layoutManager.getActiveLayoutType()
                                            + ", expected BROWSING type on activity start.";
                            LayoutStateObserver.super.onFinishedShowing(layoutType);
                            layoutStateHandler.post(
                                    () -> {
                                        onLayoutFinishedShowing.run();
                                        layoutManager.removeObserver(this);
                                        layoutStateHandler.removeCallbacksAndMessages(null);
                                    });
                        }
                    });
        }
    }

    private static void restoreOmniboxState(
            @NonNull Bundle savedInstanceState,
            ToolbarManager toolbarManager,
            @NonNull LayoutManager layoutManager,
            Handler layoutStateHandler) {
        if (toolbarManager == null || !savedInstanceState.getBoolean(URL_BAR_FOCUS_STATE, false)) {
            return;
        }
        String urlBarText = savedInstanceState.getString(URL_BAR_EDIT_TEXT, "");
        restoreUiStateOnLayoutDoneShowing(
                layoutManager,
                layoutStateHandler,
                () -> setUrlBarFocusAndText(toolbarManager, urlBarText));
    }

    private static void restoreKeyboardState(
            @NonNull Bundle savedInstanceState,
            @NonNull ActivityTabProvider activityTabProvider,
            @NonNull LayoutManager layoutManager,
            Handler layoutStateHandler) {
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

    private static void restoreTabSwitcherState(
            @NonNull Bundle savedInstanceState, @NonNull LayoutManager layoutManager) {
        if (!savedInstanceState.getBoolean(TAB_SWITCHER_VISIBILITY_STATE, false)) {
            return;
        }
        layoutManager.showLayout(LayoutType.TAB_SWITCHER, false);
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
