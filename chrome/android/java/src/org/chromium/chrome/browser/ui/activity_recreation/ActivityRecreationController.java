// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.activity_recreation;

import android.os.Bundle;
import android.os.Handler;

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

/**
 * A utility class to handle saving and restoring the UI state across fold transitions, density
 * change or UI mode type change.
 */
public class ActivityRecreationController {
    static final String ACTIVITY_RECREATION_UI_STATE = "activity_recreation_ui_state";

    private final OneshotSupplier<ToolbarManager> mToolbarManagerSupplier;
    private final ObservableSupplier<LayoutManager> mLayoutManagerSupplier;
    private final ActivityTabProvider mActivityTabProvider;
    private final Handler mLayoutStateHandler;
    private ActivityRecreationUiState mRetainedUiState;

    /**
     * Construct a {@link ActivityRecreationController} instance.
     *
     * @param toolbarManagerSupplier The {@link ToolbarManager} instance supplier.
     * @param layoutManagerSupplier The {@link LayoutManager} instance supplier.
     * @param activityTabProvider The current activity tab provider.
     * @param layoutStateHandler The {@link Handler} to post UI state restoration.
     */
    public ActivityRecreationController(
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
     * Saves the relevant UI to {@link ActivityRecreationUiState} before the activity is recreated
     * on a device fold transition, density change or UI mode type change. This preserves the actual
     * UI state, that could change before {@code Activity#onSaveInstanceState()} is called. For e.g.
     * url bar focus is cleared before {@code Activity#onSaveInstanceState()}.
     */
    public void prepareUiState() {
        mRetainedUiState = new ActivityRecreationUiState();
        if (mToolbarManagerSupplier.hasValue() && mToolbarManagerSupplier.get().isUrlBarFocused()) {
            mRetainedUiState.mIsUrlBarFocused = true;
            mRetainedUiState.mUrlBarEditText =
                    mToolbarManagerSupplier.get().getUrlBarTextWithoutAutocomplete();
        }

        if (getKeyboardVisibilityState()) {
            mRetainedUiState.mIsKeyboardShown = true;
        }

        if (mLayoutManagerSupplier.hasValue()) {
            if (mLayoutManagerSupplier.get().isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                mRetainedUiState.mIsTabSwitcherShown = true;
            }
        }
    }

    /**
     * Saves the relevant UI from {@link ActivityRecreationUiState} to {@link Bundle} when the
     * activity is recreated on a device fold transition, density change or UI mode type change.
     * Expected to be invoked during {@code Activity#onSaveInstanceState()}.
     *
     * @param savedInstanceState The {@link Bundle} where the UI state will be saved.
     */
    public void saveUiState(Bundle savedInstanceState) {
        if (savedInstanceState == null) return;

        if (mRetainedUiState == null || !mRetainedUiState.shouldRetainState()) return;
        savedInstanceState.putParcelable(ACTIVITY_RECREATION_UI_STATE, mRetainedUiState);
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition,
     * density change or UI mode type change.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     */
    public void restoreUiState(Bundle savedInstanceState) {
        if (savedInstanceState == null || !mLayoutManagerSupplier.hasValue()) {
            return;
        }

        ActivityRecreationUiState uiState =
                savedInstanceState.getParcelable(ACTIVITY_RECREATION_UI_STATE);
        if (uiState == null) {
            return;
        }
        restoreOmniboxState(
                uiState,
                mToolbarManagerSupplier.get(),
                mLayoutManagerSupplier.get(),
                mLayoutStateHandler);
        restoreKeyboardState(
                uiState, mActivityTabProvider, mLayoutManagerSupplier.get(), mLayoutStateHandler);
        restoreTabSwitcherState(uiState, mLayoutManagerSupplier.get());
    }

    private boolean getKeyboardVisibilityState() {
        if (!shouldSaveKeyboardState(mActivityTabProvider)) {
            return false;
        }

        return isKeyboardVisible(mActivityTabProvider);
    }

    /**
     * Determines whether the keyboard state should be saved during a fold transition, density
     * change or UI mode type change. The keyboard state will be saved only if the web contents has
     * a focused editable node.
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
            @NonNull ActivityRecreationUiState uiState,
            ToolbarManager toolbarManager,
            @NonNull LayoutManager layoutManager,
            Handler layoutStateHandler) {
        if (toolbarManager == null || !uiState.mIsUrlBarFocused) {
            return;
        }
        String urlBarText = uiState.mUrlBarEditText;
        restoreUiStateOnLayoutDoneShowing(
                layoutManager,
                layoutStateHandler,
                () -> setUrlBarFocusAndText(toolbarManager, urlBarText));
    }

    private static void restoreKeyboardState(
            @NonNull ActivityRecreationUiState uiState,
            @NonNull ActivityTabProvider activityTabProvider,
            @NonNull LayoutManager layoutManager,
            Handler layoutStateHandler) {
        // Restore the keyboard only if the omnibox focus was not restored, because omnibox code
        // is assumed to restore the keyboard on omnibox focus restoration.
        if (uiState.mIsUrlBarFocused || !uiState.mIsKeyboardShown) {
            return;
        }
        restoreUiStateOnLayoutDoneShowing(
                layoutManager, layoutStateHandler, () -> showSoftInput(activityTabProvider));
    }

    private static void restoreTabSwitcherState(
            @NonNull ActivityRecreationUiState uiState, @NonNull LayoutManager layoutManager) {
        if (!uiState.mIsTabSwitcherShown) {
            return;
        }
        layoutManager.showLayout(LayoutType.TAB_SWITCHER, false);
    }

    private static void setUrlBarFocusAndText(ToolbarManager toolbarManager, String urlBarText) {
        toolbarManager.setUrlBarFocusAndText(
                true, OmniboxFocusReason.ACTIVITY_RECREATION_RESTORATION, urlBarText);
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
