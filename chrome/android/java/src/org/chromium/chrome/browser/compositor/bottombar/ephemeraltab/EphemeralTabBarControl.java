// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Top control used for ephemeral tab.
 */
public class EphemeralTabBarControl {
    /** Full opacity -- fully visible. */
    private static final float SOLID_OPAQUE = 1.0f;

    /** Transparent opacity -- completely transparent (not visible). */
    private static final float SOLID_TRANSPARENT = 0.0f;

    private final EphemeralTabTitleControl mTitle;
    private final EphemeralTabCaptionControl mCaption;

    // Dimensions used for laying out the controls in the bar.
    private final float mTextLayerMinHeight;
    private final float mTitleCaptionSpacing;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param loader The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabBarControl(EphemeralTabPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader loader) {
        mTitle = new EphemeralTabTitleControl(panel, context, container, loader);
        mCaption = OverlayPanel.isNewLayout() || panel.canPromoteToNewTab()
                ? new EphemeralTabCaptionControl(panel, context, container, loader)
                : null;
        mTextLayerMinHeight =
                context.getResources().getDimension(R.dimen.overlay_panel_text_layer_min_height);
        mTitleCaptionSpacing =
                context.getResources().getDimension(R.dimen.contextual_search_term_caption_spacing);
    }

    /**
     * Returns the minimum height that the text layer (containing the title and the caption)
     * should be.
     */
    public float getTextLayerMinHeight() {
        return mTextLayerMinHeight;
    }

    /**
     * Returns the spacing that should be placed between the title and the caption.
     */
    public float getTitleCaptionSpacing() {
        return mTitleCaptionSpacing;
    }

    /**
     * Updates this bar when in transition to closed/peeked states.
     * @param percentage The percentage to the more opened state.
     */
    public void updateForCloseOrPeek(float percentage) {
        if (OverlayPanel.isNewLayout()) {
            updateForMaximize(SOLID_OPAQUE);
        } else {
            if (percentage == SOLID_OPAQUE) updateForMaximize(SOLID_TRANSPARENT);

            // When the panel is completely closed the caption should be hidden.
            if (percentage == SOLID_TRANSPARENT && mCaption != null) mCaption.hide();
        }
    }

    /**
     * Updates this bar when in transition to maximized states.
     * @param percentage The percentage to the more opened state.
     */
    public void updateForMaximize(float percentage) {
        if (mCaption != null) mCaption.updatePanelForMaximization(percentage);
    }

    /**
     * Set the text in the panel.
     * @param text The string to set the text to.
     */
    public void setBarText(String text) {
        mTitle.setBarText(text);
    }

    /**
     * @return {@link EphemeralTabTitleControl} object.
     */
    public EphemeralTabTitleControl getTitleControl() {
        return mTitle;
    }

    /**
     * @return {@link EphemeralTabCaptionControl} object.
     */
    public EphemeralTabCaptionControl getCaptionControl() {
        return mCaption;
    }

    /**
     * Gets the current animation percentage for the Caption control, which guides the vertical
     * position and opacity of the caption.
     * @return The animation percentage ranging from 0.0 to 1.0.
     *
     */
    public float getCaptionAnimationPercentage() {
        return mCaption.getAnimationPercentage();
    }

    /**
     * Removes the bottom bar views from the parent container.
     */
    public void destroy() {
        mTitle.destroy();
        if (mCaption != null) mCaption.destroy();
    }
}
