// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fold_transitions;

import android.os.Bundle;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * A utility class to handle saving and restoring the UI state across fold transitions.
 */
public class FoldTransitionController {
    @VisibleForTesting
    public static final String DID_CHANGE_TABLET_MODE = "did_change_tablet_mode";
    static final String URL_BAR_FOCUS_STATE = "url_bar_focus_state";
    static final String URL_BAR_EDIT_TEXT = "url_bar_edit_text";

    /**
     * Saves the relevant UI when the activity is recreated on a device fold transition. Expected to
     * be invoked during {@code Activity#onSaveInstanceState()}.
     *
     * @param savedInstanceState The {@link Bundle} where the UI state will be saved.
     * @param toolbarManager The {@link ToolbarManager} for the current activity.
     * @param activityTabProvider The activity tab {@link Supplier} for the current activity.
     * @param didChangeTabletMode Whether the activity is recreated due to a fold configuration
     *         change. {@code true} if the fold configuration changed, {@code false} otherwise.
     */
    public static void saveUiState(Bundle savedInstanceState, ToolbarManager toolbarManager,
            Supplier<Tab> activityTabProvider, boolean didChangeTabletMode) {
        savedInstanceState.putBoolean(
                FoldTransitionController.DID_CHANGE_TABLET_MODE, didChangeTabletMode);
        saveOmniboxState(savedInstanceState, toolbarManager, activityTabProvider);
    }

    /**
     * Restores the relevant UI state when the activity is recreated on a device fold transition.
     *
     * @param savedInstanceState The {@link Bundle} that is used to restore the UI state.
     * @param toolbarManager The {@link ToolbarManager} for the current activity.
     * @param layoutManager The {@link LayoutStateProvider} for the current activity.
     * @param layoutStateHandler The {@link Handler} to post UI state restoration.
     */
    public static void restoreUiState(Bundle savedInstanceState, ToolbarManager toolbarManager,
            LayoutStateProvider layoutManager, Handler layoutStateHandler) {
        restoreOmniboxState(savedInstanceState, toolbarManager, layoutManager, layoutStateHandler);
    }

    private static void saveOmniboxState(Bundle savedInstanceState, ToolbarManager toolbarManager,
            Supplier<Tab> activityTabProvider) {
        if (savedInstanceState == null || toolbarManager == null) {
            return;
        }
        if (activityTabProvider == null || activityTabProvider.get() == null
                || UrlUtilities.isNTPUrl(activityTabProvider.get().getUrl())) {
            // TODO (crbug.com/1425248): Support NTP fakebox focus state retention.
            return;
        }
        if (toolbarManager.isUrlBarFocused()) {
            savedInstanceState.putBoolean(FoldTransitionController.URL_BAR_FOCUS_STATE, true);
            savedInstanceState.putString(FoldTransitionController.URL_BAR_EDIT_TEXT,
                    toolbarManager.getUrlBarTextWithoutAutocomplete());
        }
    }

    private static void restoreOmniboxState(Bundle savedInstanceState,
            ToolbarManager toolbarManager, LayoutStateProvider layoutManager,
            Handler layoutStateHandler) {
        if (savedInstanceState == null || toolbarManager == null || layoutManager == null) {
            return;
        }
        // Restore the omnibox state only on a device fold transition.
        if (!savedInstanceState.getBoolean(DID_CHANGE_TABLET_MODE, false)) {
            return;
        }

        if (savedInstanceState.getBoolean(URL_BAR_FOCUS_STATE, false)) {
            String urlBarText = savedInstanceState.getString(URL_BAR_EDIT_TEXT, "");

            /* TODO (crbug/1395495): Call {@code ToolbarManager#setUrlBarFocusAndText(boolean,
             * @OmniboxFocusReason int, String)} directly if the invocation of {@code
             * StaticLayout#requestFocus(Tab)} in {@code StaticLayout#doneShowing()} is removed. We
             * set focus currently after the {@link StaticLayout} is done showing to persist the
             * last focus on the omnibox. If the layout is visible and done showing, it is safe to
             * call {@code ToolbarManager#setUrlBarFocusAndText(boolean, @OmniboxFocusReason int,
             * String)} directly to persist the omnibox focus. */
            if (layoutManager.isLayoutVisible(LayoutType.BROWSING)
                    && !layoutManager.isLayoutStartingToShow(LayoutType.BROWSING)) {
                setUrlBarFocusAndText(toolbarManager, urlBarText);
            } else {
                layoutManager.addObserver(createLayoutStateObserver(
                        urlBarText, toolbarManager, layoutManager, layoutStateHandler));
            }
        }
    }

    @VisibleForTesting
    static LayoutStateObserver createLayoutStateObserver(String urlBarText,
            ToolbarManager toolbarManager, LayoutStateProvider layoutManager,
            Handler layoutStateHandler) {
        return new LayoutStateObserver() {
            @Override
            public void onFinishedShowing(int layoutType) {
                assert layoutManager.isLayoutVisible(LayoutType.BROWSING)
                    : "LayoutType is "
                        + layoutManager.getActiveLayoutType()
                        + ", expected BROWSING type on activity start.";
                LayoutStateObserver.super.onFinishedShowing(layoutType);
                layoutStateHandler.post(() -> {
                    setUrlBarFocusAndText(toolbarManager, urlBarText);
                    layoutManager.removeObserver(this);
                    layoutStateHandler.removeCallbacksAndMessages(null);
                });
            }
        };
    }

    private static void setUrlBarFocusAndText(ToolbarManager toolbarManager, String urlBarText) {
        toolbarManager.setUrlBarFocusAndText(
                true, OmniboxFocusReason.FOLD_TRANSITION_RESTORATION, urlBarText);
    }
}
