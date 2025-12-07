// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A delegate that handles the presentation logic for all {@link StripLayoutTab} instances. */
@NullMarked
public class StripLayoutTabDelegate {
    // Opacity
    public static final float TAB_OPACITY_HIDDEN = 0.f;
    public static final float TAB_OPACITY_VISIBLE = 1.f;
    // Lifted effect for hovered tabs.
    public static final float FOLIO_ATTACHED_BOTTOM_MARGIN_DP = 0.f;
    public static final float FOLIO_DETACHED_BOTTOM_MARGIN_DP = 4.f;

    public static final float TAB_WIDTH_MEDIUM = 156.f;

    private static final float CLOSE_BTN_VISIBILITY_THRESHOLD_START =
            StripLayoutUtils.shouldApplyMoreDensity() ? 64.f : 96.f;

    public static final int ANIM_HOVERED_TAB_CONTAINER_FADE_MS = 200;
    private final LayoutUpdateHost mUpdateHost;

    /** Defines the different visual states a tab can be in, used for determining its tint. */
    @IntDef({
        VisualState.NORMAL,
        VisualState.PLACEHOLDER,
        VisualState.HOVERED,
        VisualState.MULTISELECT,
        VisualState.MULTISELECT_HOVERED,
        VisualState.SELECTED,
        VisualState.SELECTED_HOVERED,
        VisualState.NON_DRAG_REORDERING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VisualState {
        /** The default state for a tab that has no other state applied. */
        int NORMAL = 0;

        /** A temporary state for a tab that is waiting for its data to be loaded on startup. */
        int PLACEHOLDER = 1;

        /** The state when a pointer is hovering over an unselected tab. */
        int HOVERED = 2;

        /** The state for a tab that is part of a multi-selection, but is not active or hovered. */
        int MULTISELECT = 3;

        /**
         * The state when a pointer is hovering over a tab that is also part of a multi-selection.
         */
        int MULTISELECT_HOVERED = 4;

        /** The state for the currently active tab. */
        int SELECTED = 5;

        /** The state when a pointer is hovering over the currently active tab. */
        int SELECTED_HOVERED = 6;

        /** The state when the tab is reordering for a non-drag operation. */
        int NON_DRAG_REORDERING = 7;
    }

    /**
     * Constructs a new delegate to handle tab presentation logic.
     *
     * @param updateHost The {@link LayoutUpdateHost} for requesting animations and updates.
     */
    public StripLayoutTabDelegate(LayoutUpdateHost updateHost) {
        mUpdateHost = updateHost;
    }

    /**
     * Determines if the close button for a tab should be visible based on its state and position.
     *
     * @param tab The {@link StripLayoutTab} to update.
     * @param isLastTab Whether this is the last tab in the strip.
     * @param stripLeftFadeWidth The width of the fade overlay on the left side of the strip.
     * @param stripRightFadeWidth The width of the fade overlay on the right side of the strip.
     * @param visibleLeftBound The strip's visible left bound, in dps.
     * @param visibleRightBound The strip's visible right bound, in dps.
     * @param newTabButton The New Tab Button, used for positioning calculations.
     * @param isFirstLayoutPass Whether this is the first layout pass, used to suppress animations.
     * @return Whether the close button changed visibility.
     */
    public boolean updateTabCloseButtonVisibility(
            StripLayoutTab tab,
            boolean isLastTab,
            float stripLeftFadeWidth,
            float stripRightFadeWidth,
            float visibleLeftBound,
            float visibleRightBound,
            TintedCompositorButton newTabButton,
            boolean isFirstLayoutPass) {
        // A tab's close button is hidden if the tab is too narrow, or if it is partially obscured
        // by the edge of the screen, unless it is also the selected tab.
        boolean isFullyVisible =
                isFullyVisible(
                        tab,
                        isLastTab,
                        stripLeftFadeWidth,
                        stripRightFadeWidth,
                        visibleLeftBound,
                        visibleRightBound,
                        newTabButton);

        boolean currentCanShow = tab.canShowCloseButton();
        boolean canShow =
                !tab.getIsPinned()
                        && (tab.getWidth() >= TAB_WIDTH_MEDIUM
                                || (tab.getIsSelected() && isFullyVisible));

        // A dying tab that is not selected should not show its close button.
        // TODO(crbug.com/419843587): Await UX direction for close button appearance
        if (ChromeFeatureList.sTabletTabStripAnimation.isEnabled()
                && tab.isDying()
                && !tab.getIsSelected()) {
            canShow = false;
            tab.setCanShowCloseButton(canShow, false);
        } else {
            tab.setCanShowCloseButton(canShow, !isFirstLayoutPass);
        }
        return currentCanShow != canShow;
    }

    /**
     * Sets the selected state for this tab and updates its visual appearance instantly.
     *
     * @param tab The {@link StripLayoutTab} the to modify.
     * @param isSelected Whether the tab is selected.
     */
    public void setIsTabSelected(StripLayoutTab tab, boolean isSelected) {
        tab.setIsSelected(isSelected);
        updateTabVisualState(tab, false);
    }

    /**
     * Sets the hovered state for this tab.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isHovered Whether the tab is hovered.
     */
    public void setIsTabHovered(StripLayoutTab tab, boolean isHovered) {
        tab.setIsHovered(isHovered);
        if (!isHovered) tab.setCloseHovered(false);
        updateTabVisualState(tab, isHovered);
    }

    /**
     * Sets the placeholder state for a given tab.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isPlaceholder Whether the tab is a placeholder.
     */
    public void setIsTabPlaceholder(StripLayoutTab tab, boolean isPlaceholder) {
        tab.setIsPlaceholder(isPlaceholder);
        updateTabVisualState(tab, false);
    }

    /**
     * Sets the multi-selection state for a given tab.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isMultiSelected Whether the tab is part of a multi-selection.
     */
    public void setIsTabMultiSelected(StripLayoutTab tab, boolean isMultiSelected) {
        setIsTabMultiSelected(tab, isMultiSelected, isMultiSelected);
    }

    /**
     * Sets the multi-selection state for a given tab.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isMultiSelected Whether the tab is part of a multi-selection.
     * @param animate Whether to animate the resulting opacity changes.
     */
    public void setIsTabMultiSelected(
            StripLayoutTab tab, boolean isMultiSelected, boolean animate) {
        tab.setIsMultiSelected(isMultiSelected);
        updateTabVisualState(tab, animate);
    }

    /**
     * Sets the non-drag reordering state for a given tab.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isNonDragReordering Whether the tab is reordering for a non-drag operation.
     */
    public void setIsTabNonDragReordering(StripLayoutTab tab, boolean isNonDragReordering) {
        tab.setIsNonDragReordering(isNonDragReordering);
        updateTabVisualState(tab, /* animate= */ false);
    }

    /**
     * Checks if the tab's background container is fully transparent.
     *
     * @param tab The {@link StripLayoutTab} to check.
     * @return True if the tab container is hidden.
     */
    public static boolean isTabHidden(StripLayoutTab tab) {
        return tab.getContainerOpacity() == TAB_OPACITY_HIDDEN;
    }

    /**
     * Checks if the tab's background container is fully opaque.
     *
     * @param tab The {@link StripLayoutTab} to check.
     * @return True if the tab container is visible.
     */
    public static boolean isTabVisible(StripLayoutTab tab) {
        return tab.getContainerOpacity() == TAB_OPACITY_VISIBLE;
    }

    /** Updates a tab's visibility based on its internal state. */
    public static void updateTabVisibility(StripLayoutTab tab) {
        // TODO(crbug.com/436663313): Stop exposing this method, and handle solely in
        //  #updateTabVisualState.
        setTabVisibility(tab, tab.shouldBeVisible());
    }

    /**
     * Sets the visibility of the tab's background container directly.
     *
     * @param tab The {@link StripLayoutTab} to modify.
     * @param isVisible Whether the container should be visible.
     */
    public static void setTabVisibility(StripLayoutTab tab, boolean isVisible) {
        // TODO(crbug.com/436663313): Stop exposing this method, and handle solely in
        //  #updateTabVisualState.
        float containerOpacity = isVisible ? TAB_OPACITY_VISIBLE : TAB_OPACITY_HIDDEN;
        tab.setContainerOpacity(containerOpacity);
    }

    /**
     * Updates the hover state of a tab's close button based on the pointer's coordinates.
     *
     * @param tab The {@link StripLayoutTab} whose close button will be updated.
     * @param x The x-coordinate of the pointer.
     * @param y The y-coordinate of the pointer.
     * @return {@link Boolean} Whether or not the close button hover state changed.
     */
    public static boolean updateTabCloseHoverState(StripLayoutTab tab, float x, float y) {
        boolean isCloseHit = tab.checkCloseHitTest(x, y);
        if (isCloseHit == tab.isCloseHovered()) return false;
        tab.setCloseHovered(isCloseHit);
        return true;
    }

    /** Central method to update a tab's appearance based on its state. */
    private void updateTabVisualState(StripLayoutTab tab, boolean animate) {
        @VisualState int visualState = calculateVisualState(tab);
        if (tab.getVisualState() == visualState) return;
        tab.setVisualState(visualState);
        // 1. Update Container Opacity.
        boolean shouldBeOpaque = visualState != VisualState.NORMAL;

        if (animate) {
            float targetOpacity = shouldBeOpaque ? TAB_OPACITY_VISIBLE : TAB_OPACITY_HIDDEN;
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            tab,
                            StripLayoutTab.OPACITY,
                            tab.getContainerOpacity(),
                            targetOpacity,
                            ANIM_HOVERED_TAB_CONTAINER_FADE_MS)
                    .start();
        } else {
            setTabVisibility(tab, shouldBeOpaque);
        }

        // 2. Update the "folio" lifted effect.
        // It should only apply to non-selected tabs.
        if (!tab.getIsSelected()) {
            tab.setFolioAttached(/* folioAttached= */ false);
            tab.setBottomMargin(FOLIO_DETACHED_BOTTOM_MARGIN_DP);
        } else {
            // Ensure selected tabs are "attached".
            tab.setFolioAttached(/* folioAttached= */ true);
            tab.setBottomMargin(FOLIO_ATTACHED_BOTTOM_MARGIN_DP);
        }
    }

    /** Calculates if the tab is fully visible and not partially hidden off-screen. */
    private static boolean isFullyVisible(
            StripLayoutTab tab,
            boolean isLastTab,
            float stripLeftFadeWidth,
            float stripRightFadeWidth,
            float visibleLeftBound,
            float visibleRightBound,
            TintedCompositorButton newTabButton) {
        boolean tabStartHidden;
        boolean tabEndHidden;

        if (LocalizationUtils.isLayoutRtl()) {
            // For RTL, the "start" of the tab is its right edge and the "end" is its left.
            if (isLastTab) {
                // The last tab is positioned next to the new tab button.
                tabStartHidden =
                        tab.getDrawX() + TAB_OVERLAP_WIDTH_DP
                                < visibleLeftBound
                                        + newTabButton.getDrawX()
                                        + newTabButton.getWidth();
            } else {
                tabStartHidden =
                        tab.getDrawX() + TAB_OVERLAP_WIDTH_DP
                                < visibleLeftBound + stripLeftFadeWidth;
            }
            tabEndHidden =
                    tab.getDrawX() > visibleRightBound - CLOSE_BTN_VISIBILITY_THRESHOLD_START;
        } else {
            // For LTR, the "start" of the tab is its left edge and the "end" is its right.
            tabStartHidden =
                    tab.getDrawX() + tab.getWidth()
                            < visibleLeftBound + CLOSE_BTN_VISIBILITY_THRESHOLD_START;
            if (isLastTab) {
                // The last tab is positioned next to the new tab button.
                tabEndHidden =
                        tab.getDrawX() + tab.getWidth() - TAB_OVERLAP_WIDTH_DP
                                > visibleLeftBound + newTabButton.getDrawX();
            } else {
                tabEndHidden =
                        (tab.getDrawX() + tab.getWidth() - TAB_OVERLAP_WIDTH_DP
                                > visibleRightBound - stripRightFadeWidth);
            }
        }
        return !tabStartHidden && !tabEndHidden;
    }

    /**
     * Determines the visual state of a tab based on several properties (e.g. selected, hovered,
     * multi-selected, placeholder, non-drag-reordering, etc.).
     */
    private static @VisualState int calculateVisualState(StripLayoutTab tab) {
        if (tab.getIsSelected() && tab.getIsHovered()) {
            return VisualState.SELECTED_HOVERED;
        } else if (tab.getIsSelected()) {
            return VisualState.SELECTED;
        } else if (tab.getIsNonDragReordering()) {
            return VisualState.NON_DRAG_REORDERING;
        } else if (tab.getIsMultiSelected() && tab.getIsHovered()) {
            return VisualState.MULTISELECT_HOVERED;
        } else if (tab.getIsMultiSelected()) {
            return VisualState.MULTISELECT;
        } else if (tab.getIsHovered()) {
            return VisualState.HOVERED;
        } else if (tab.getIsPlaceholder()) {
            return VisualState.PLACEHOLDER;
        } else {
            return VisualState.NORMAL;
        }
    }
}
