// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

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

    /** Whether this control is enabled or not. */
    private final boolean mIsEnabled;

    /** Whether the alternate text variant is enabled or not. */
    private final boolean mIsTextVariantEnabled;

    @Nullable private TextView mTextView;

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
            ViewGroup container,
            DynamicResourceLoader resourceLoader,
            CalloutListener listener) {
        super(
                panel,
                R.layout.contextual_search_callout_view,
                R.id.contextual_search_callout,
                context,
                container,
                resourceLoader);
        mListener = listener;
        mIsEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.TOUCH_TO_SEARCH_CALLOUT);
        mIsTextVariantEnabled = ChromeFeatureList.sTouchToSearchCalloutTextVariant.getValue();

        // Pre-inflate so that the contextual search text padding is adjusted to the callout width.
        if (mIsEnabled) {
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
        // The callout animation completes during the first 50% of the peek to expand transition.
        float animationProgressPercent = Math.min(percentage, .5f) / .5f;
        // The callout starts off at 100% opacity and finishes at 0% opacity.
        mOpacity = 1.f - animationProgressPercent;
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

        mTextView = view.findViewById(R.id.contextual_search_callout_text);
        mTextView.setText(
                mIsTextVariantEnabled
                        ? view.getResources()
                                .getString(R.string.contextual_search_callout_text_variant)
                        : view.getResources().getString(R.string.contextual_search_callout_text));

        mImageView = view.findViewById(R.id.contextual_search_callout_image);
    }

    @Override
    protected void onCaptureEnd() {
        super.onCaptureEnd();

        Context context = assumeNonNull(getContext());
        mCalloutWidthPx =
                (int)
                        (assumeNonNull(mTextView).getWidth()
                                + assumeNonNull(mImageView).getWidth()
                                + context.getResources()
                                        .getDimension(
                                                R.dimen.contextual_search_callout_margin_start)
                                + context.getResources()
                                        .getDimension(
                                                R.dimen.contextual_search_callout_margin_end));

        mListener.onCapture(mCalloutWidthPx);
    }
}
