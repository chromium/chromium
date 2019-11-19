// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.Rect;
import android.view.View;
import android.widget.ProgressBar;

import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;

/**
 * An interface for outside packages to interact with the top toolbar. Other than for testing
 * purposes this interface should be used rather than {@link TopToolbarCoordinator} or
 * {@link ToolbarLayout} and extending classes.
 */
public interface Toolbar {
    /**
     * Calculates the {@link Rect} that represents the content area of the location bar.  This
     * rect will be relative to the toolbar.
     * @param outRect The Rect that represents the content area of the location bar.
     */
    void getLocationBarContentRect(Rect outRect);

    /**
     * @return Whether any swipe gestures should be ignored for the current Toolbar state.
     */
    boolean shouldIgnoreSwipeGesture();

    /**
     * Calculate the relative position wrt to the given container view.
     * @param containerView The container view to be used.
     * @param position The position array to be used for returning the calculated position.
     */
    void getPositionRelativeToContainer(View containerView, int[] position);

    /**
     * Get the height of the toolbar in px.
     * @return The height of the toolbar.
     */
    int getHeight();

    /**
     * Sets whether or not the toolbar should draw as if it's being captured for a snapshot
     * texture.  In this mode it will only draw the toolbar in it's normal state (no TabSwitcher
     * or animations).
     * @param textureMode Whether or not to be in texture capture mode.
     */
    void setTextureCaptureMode(boolean textureMode);

    /**
     * @return Whether a dirty check for invalidation makes sense at this time.
     */
    boolean isReadyForTextureCapture();

    /**
     * Sets whether or not the toolbar should force itself to draw for a texture capture regardless
     * of other criteria used in isReadyForTextureCapture(). A texture capture will only be forced
     * if the toolbar drawables tint is changing.
     *
     * @param forceTextureCapture Whether the toolbar should force itself to draw.
     * @return True if a texture capture will be forced on the next draw.
     */
    boolean setForceTextureCapture(boolean forceTextureCapture);

    /**
     * Sets whether or not the menu button should be highlighted.
     * @param highlight Whether or not the menu button should be highlighted.
     */
    void setMenuButtonHighlight(boolean highlight);

    /**
     * Show the update badge on the app menu button. Will have no effect if there is no app menu
     * button for the current Activity.
     */
    void showAppMenuUpdateBadge();

    /**
     * Whether the update badge that is displayed on top of the app menu button is showing.
     */
    boolean isShowingAppMenuUpdateBadge();

    /**
     * Remove the update badge on the app menu button. Initially the badge is invisible so that it
     * gets measured and the tab switcher animation looks correct when the badge is first shown. If
     * the badge will never be shown or should no longer be shown, this method should be called to
     * change the visibility to gone to avoid unnecessary layout work. The disappearance of the
     * badge is optionally animated if it was previously visible.
     *
     * @param animate Whether the removal of the badge should be animated.
     */
    void removeAppMenuUpdateBadge(boolean animate);

    /**
     * Returns the height of the tab strip, iff the toolbar has one. Returns 0 for toolbars that do
     * not have a tabstrip.
     * @return height of the tab strip in px.
     */
    int getTabStripHeight();

    /**
     * Disable the menu button. This removes the view from the hierarchy and nulls the related
     * instance vars.
     */
    void disableMenuButton();

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
    ToolbarProgressBar getProgressBar();

    /**
     * @return The primary color to use for the background drawable.
     */
    int getPrimaryColor();

    /**
     * Update the Tab switcher toolbar state.
     * @param requestToShow Whether or not request showing the Tab switcher toolbar.
     */
    void updateTabSwitcherToolbarState(boolean requestToShow);
}
