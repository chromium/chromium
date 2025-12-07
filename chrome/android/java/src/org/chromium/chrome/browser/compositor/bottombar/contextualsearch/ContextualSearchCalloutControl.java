// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** Controls the callout (e.g. "Latest results") shown in the {@link ContextualSearchBarControl}. */
@NullMarked
public class ContextualSearchCalloutControl extends OverlayPanelInflater {

    interface CalloutListener {
        /** Called when the callout is rendered. */
        void onCapture(int widthPx);
    }

    /** Listener for updates to the callout. */
    private final CalloutListener mListener;

    /**
     * Whether the super G callout icon is enabled. It is hidden unless a custom image is shown on
     * the left of the bar, to prevent two super G icons from being displayed at the same time.
     */
    private boolean mIsIconEnabled;

    @Nullable private ImageView mImageView;

    private int mCalloutWidthPx;

    private float mOpacity;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     * @param listener The listener for capturing callout resize events.
     */
    public ContextualSearchCalloutControl(
            ContextualSearchPanel panel,
            Context context,
            @Nullable ViewGroup container,
            @Nullable DynamicResourceLoader resourceLoader,
            CalloutListener listener) {
        super(
                panel,
                R.layout.contextual_search_callout_view,
                R.id.contextual_search_callout,
                context,
                container,
                resourceLoader);
        mListener = listener;

        // The callout icon feature is only enabled when IPH is disabled.
        boolean isCalloutIconFeatureEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.TOUCH_TO_SEARCH_CALLOUT)
                        && !ChromeFeatureList.sTouchToSearchCalloutIph.getValue();
        // Pre-inflate so that the contextual search text padding is adjusted to the callout width.
        if (isCalloutIconFeatureEnabled) {
            inflate();
            invalidate();
        }
    }

    /**
     * Updates the opacity of the callout based on the panel expansion.
     *
     * @param percentage The percentage of the panel that is expanded.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        if (!mIsIconEnabled) {
            return;
        }

        // The callout animation completes during the first 50% of the peek to expand transition.
        float animationProgressPercent = Math.min(percentage, .5f) / .5f;
        // The callout starts off at 100% opacity and finishes at 0% opacity.
        mOpacity = 1.f - animationProgressPercent;
    }

    /**
     * Updates the opacity of the callout when the TTS panel is updated from a super G to a custom
     * thumbnail.
     *
     * @param percentage The percentage of the fade in animation.
     */
    public void onUpdateCustomImageVisibility(
            boolean customImageIsVisible, float visibilityPercentage) {
        // Enables the super G icon callout when the custom image becomes visible.
        mIsIconEnabled = customImageIsVisible;
        mOpacity = visibilityPercentage;
    }

    /**
     * @return The opacity of the callout.
     */
    public float getOpacity() {
        return mOpacity;
    }

    // ========================================================================================
    // OverlayPanelInflater overrides
    // ========================================================================================

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = assumeNonNull(getView());

        mImageView = view.findViewById(R.id.contextual_search_callout_image);
    }

    @Override
    protected void onCaptureEnd() {
        super.onCaptureEnd();

        Context context = assumeNonNull(getContext());
        int imageMargin =
                (int)
                        (2.f
                                * context.getResources()
                                        .getDimension(
                                                R.dimen.contextual_search_callout_icon_margin));
        mCalloutWidthPx = (int) (assumeNonNull(mImageView).getWidth() + imageMargin);

        mListener.onCapture(mCalloutWidthPx);
    }
}
