// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.base.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelTextViewInflater;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Caption View that is shown at the bottom of the control and used
 * as a dynamic resource.
 */
public class EphemeralTabCaptionControl extends OverlayPanelTextViewInflater {
    /** Space for security icon. Caption is pushed to right by this amount if the icon is shown. */
    private final int mIconMargin;

    /** The caption View. */
    private TextView mCaption;

    /** Whether the caption is showing. */
    private boolean mShowingCaption;

    /** The caption visibility. */
    private boolean mIsVisible;

    private Supplier<String> mUrl;

    /**
     * The caption animation percentage, which controls how and where to draw. It is
     * 0 when the Contextual Search bar is peeking and 1 when it is maxmized.
     */
    private float mAnimationPercentage;

    private @DrawableRes int mIconId;
    private float mIconOpacity;

    /**
     * @param panel                     The panel.
     * @param context                   The Android Context used to inflate the View.
     * @param container                 The container View used to inflate the View.
     * @param resourceLoader            The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabCaptionControl(EphemeralTabPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.ephemeral_tab_caption_view, R.id.ephemeral_tab_caption_view, context,
                container, resourceLoader,
                (OverlayPanel.isNewLayout() ? R.dimen.overlay_panel_end_buttons_width
                                            : R.dimen.overlay_panel_padded_button_width),
                (OverlayPanel.isNewLayout() ? R.dimen.overlay_panel_end_buttons_width
                                            : R.dimen.overlay_panel_padded_button_width));
        mUrl = panel::getUrl;
        mIconMargin = context.getResources().getDimensionPixelSize(
                R.dimen.preview_tab_security_icon_size);
    }

    /**
     * Updates the caption when in transition between peeked to maximized states.
     * @param percentage The percentage to the more opened state.
     */
    public void updatePanelForMaximization(float percentage) {
        // If the caption is not showing, show it now.
        if (!mShowingCaption && percentage > 0.f) {
            mShowingCaption = true;

            if (mCaption == null) {
                // |mCaption| gets initialized synchronously in |onFinishInflate|.
                inflate();
                if (OverlayPanel.isNewLayout()) {
                    mCaption.setText(
                            UrlFormatter.formatUrlForSecurityDisplayOmitScheme(mUrl.get()));
                } else {
                    mCaption.setText(R.string.contextmenu_open_in_new_tab);
                }
            }
            invalidate();
            mIsVisible = true;
        }

        mAnimationPercentage = percentage;
        if (mAnimationPercentage == 0.f) mShowingCaption = false;
    }

    /** Sets the security icon. */
    public void setSecurityIcon(@DrawableRes int resId) {
        mIconId = resId;
    }

    /** @return Security icon resource ID */
    public @DrawableRes int getIconId() {
        return mIconId;
    }

    /** Sets the security icon opacity. */
    public void setIconOpacity(float opacity) {
        mIconOpacity = opacity;
    }

    /** @return Security icon opacity. */
    public float getIconOpacity() {
        return mIconOpacity;
    }

    /**
     * Hides the caption.
     */
    public void hide() {
        if (mShowingCaption) {
            mIsVisible = false;
            mAnimationPercentage = 0.f;
        }
    }

    /**
     * Controls whether the caption is visible and can be rendered.
     * The caption must be visible in order to draw it and take a snapshot.
     * Even though the caption is visible the user might not be able to see it due to a
     * completely transparent opacity associated with an animation percentage of zero.
     * @return Whether the caption is visible or not.
     */
    public boolean getIsVisible() {
        return mIsVisible;
    }

    /**
     * Gets the animation percentage which controls the drawing of the caption and how high to
     * position it in the Bar.
     * @return The current percentage ranging from 0.0 to 1.0.
     */
    public float getAnimationPercentage() {
        return OverlayPanel.isNewLayout() ? 1.f : mAnimationPercentage;
    }

    /**
     * Sets caption text.
     * @param text String to use for caption.
     */
    public void setCaptionText(String text) {
        if (mCaption == null) return;
        mCaption.setText(text);
        invalidate();
    }

    // OverlayPanelTextViewInflater

    @Override
    protected TextView getTextView() {
        return mCaption;
    }

    // OverlayPanelInflater

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mCaption = (TextView) view.findViewById(R.id.ephemeral_tab_caption);
        if (OverlayPanel.isNewLayout()) {
            ((MarginLayoutParams) mCaption.getLayoutParams()).leftMargin = mIconMargin;
        }
    }
}
