// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.Toolbar;
import org.chromium.chrome.browser.toolbar.ToolbarActionModeCallback;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.base.WindowAndroid;

/**
 * Container that holds the {@link UrlBar} and SSL state related with the current {@link Tab}.
 */
public interface LocationBar extends UrlBarDelegate {

    /**
     * Handles native dependent initialization for this class.
     */
    void onNativeLibraryReady();

    /**
     * Triggered when the current tab has changed to a {@link NewTabPage}.
     */
    void onTabLoadingNTP(NewTabPage ntp);

    /**
     * Called to set the autocomplete profile to a new profile.
     */
    void setAutocompleteProfile(Profile profile);

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    void updateVisualsForState();

    /**
     * Sets the displayed URL to be the URL of the page currently showing.
     *
     * <p>The URL is converted to the most user friendly format (removing HTTP:// for example).
     *
     * <p>If the current tab is null, the URL text will be cleared.
     */
    void setUrlToPageUrl();

    /**
     * Sets the displayed title to the page title.
     */
    void setTitleToPageTitle();

    /**
     * Sets whether the location bar should have a layout showing a title.
     * @param showTitle Whether the title should be shown.
     */
    void setShowTitle(boolean showTitle);

    /**
     * Update the visuals based on a loading state change.
     * @param updateUrl Whether to update the URL as a result of the this call.
     */
    void updateLoadingState(boolean updateUrl);

    /**
     * Sets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state.
     */
    void setToolbarDataProvider(ToolbarDataProvider model);

    /**
     * Gets the {@link ToolbarDataProvider} to be used for accessing {@link Toolbar} state.
     */
    ToolbarDataProvider getToolbarDataProvider();

    /**
     * Initialize controls that will act as hooks to various functions.
     * @param windowDelegate {@link WindowDelegate} that will provide {@link Window} related info.
     * @param windowAndroid {@link WindowAndroid} that is used by the owning {@link Activity}.
     */
    void initializeControls(WindowDelegate windowDelegate, WindowAndroid windowAndroid);

    /**
     * Adds a URL focus change listener that will be notified when the URL gains or loses focus.
     * @param listener The listener to be registered.
     */
    default void addUrlFocusChangeListener(UrlFocusChangeListener listener) {}

    /**
     * Removes a URL focus change listener that was previously added.
     * @param listener The listener to be removed.
     */
    default void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {}

    /**
     * Signal a {@link UrlBar} focus change request.
     * @param shouldBeFocused Whether the focus should be requested or cleared. True requests focus
     *        and False clears focus.
     */
    void setUrlBarFocus(boolean shouldBeFocused);

    /**
     * Triggers the cursor to be visible in the UrlBar without triggering any of the focus animation
     * logic.
     * <p>
     * Only applies to devices with a hardware keyboard attached.
     */
    void showUrlBarCursorWithoutFocusAnimations();

    /**
     * @return Whether the UrlBar currently has focus.
     */
    boolean isUrlBarFocused();

    /**
     * Selects all of the editable text in the UrlBar.
     */
    void selectAll();

    /**
     * Reverts any pending edits of the location bar and reset to the page state.  This does not
     * change the focus state of the location bar.
     */
    void revertChanges();

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    void updateSecurityIcon();

    /**
     * @return The {@link ViewGroup} that this container holds.
     */
    View getContainerView();

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    View getSecurityIconView();

    /**
     * Updates the state of the mic button if there is one.
     */
    void updateMicButtonState();

    /**
     * Sets the callback to be used by default for text editing action bar.
     * @param callback The callback to use.
     */
    void setDefaultTextEditActionModeCallback(ToolbarActionModeCallback callback);

    /**
     * @return The margin to be applied to the URL bar based on the buttons currently visible next
     *         to it, used to avoid text overlapping the buttons and vice versa.
     */
    int getUrlContainerMarginEnd();

    /**
     * @param scrim The scrim for this location bar to use.
     */
    void setScrim(ScrimView scrim);

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     *
     * Immediately after the animation to transition the URL bar from focused to unfocused finishes,
     * the layout width returned from #getMeasuredWidth() can differ from the final unfocused width
     * (e.g. this value) until the next layout pass is complete.
     *
     * This value may be used to determine whether optional child views should be visible in the
     * unfocused location bar.
     *
     * @param unfocusedWidth The unfocused location bar width.
     */
    void setUnfocusedWidth(float unfocusedWidth);
}
