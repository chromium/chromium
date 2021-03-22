// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.app.Activity;

import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelInterface;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;

/**
 * Provides an interface to allow external objects like the {@link ContextualSearchPanel} to drive
 * specific actions in the {@link ContextualSearchManager} e.g tell it to close or promote the
 * panel into a separate Tab.
 */
public interface ContextualSearchManagementDelegate {

    /**
     * @return The ChromeActivity that associated with the manager.
     */
    Activity getActivity();

    /**
     * Promotes the current Content View Core in the Contextual Search Panel to its own Tab.
     */
    void promoteToTab();

    /**
     * Sets the handle to the ContextualSearchPanel.
     * @param panel The ContextualSearchPanel.
     */
    void setContextualSearchPanel(ContextualSearchPanelInterface panel);

    /**
     * Gets whether the device is running in compatibility mode for Contextual Search.
     * If so, a new tab showing search results should be opened instead of showing the panel.
     * @return whether the device is running in compatibility mode.
     */
    boolean isRunningInCompatibilityMode();

    /**
     * Opens the resolved search URL in a new tab. It is used when Contextual Search is in
     * compatibility mode.
     */
    void openResolvedSearchUrlInNewTab();

    /**
     * Dismisses the Contextual Search bar completely.  This will hide any panel that's currently
     * showing as well as any bar that's peeking.
     */
    void dismissContextualSearchBar();

    /**
     * Hides the Contextual Search UX by changing into the IDLE state.
     * @param reason The {@link StateChangeReason} for hiding Contextual Search.
     */
    void hideContextualSearch(@StateChangeReason int reason);

    /**
     * Notifies that the Contextual Search Panel did get closed.
     * @param reason The reason the panel is closing.
     */
    void onCloseContextualSearch(@StateChangeReason int reason);

    /**
     * @return An OverlayContentDelegate to watch events on the panel's content.
     */
    OverlayContentDelegate getOverlayContentDelegate();

    /**
     * Log the current state of Contextual Search.
     */
    void logCurrentState();

    /**
     * Called when the Contextual Search panel's animation is finished and it's shown.
     */
    void onPanelFinishedShowing();

    /**
     * Called when the Contextual Search panel is resized.
     */
    void onPanelResized();

    /**
     * Called when the privacy Opt-in in the panel has been accepted.
     */
    void onPromoOptIn();

    /** Returns whether the In-Panel-Help is currently enabled. */
    boolean isPanelHelpEnabled();

    /**
     * Called when the Help section of the panel has its OK button clicked.
     */
    void onPanelHelpOkClicked();

    /**
     * Notifies that a Related Searches suggestion has been clicked.
     * @param suggestionIndex The 0-based index into the list of suggestions provided by the
     *        panel and presented in the UI. E.g. if the user clicked the second chip this value
     *        would be 1.
     */
    void onRelatedSearchesSuggestionClicked(int suggestionIndex);

    /**
     * @return A {@link ScrimCoordinator} to fade the status bar in and out.
     */
    ScrimCoordinator getScrimCoordinator();
}
