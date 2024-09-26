// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Provides a TextView whose end padding can be adjusted between a short and long size.
 * This implementation simply does a binary transition when the panel is 50% of the way
 * between peek and expanded states.
 */
public abstract class OverlayPanelRepaddingTextView extends OverlayPanelInflater {
    private static final float REPADDING_THRESHOLD = 0.5f;

    private final float mPeekedEndButtonsWidth;
    private final float mExpandedEndButtonsWidth;

    private int mPaddingStart;
    private int mPaddingTop;
    private int mPaddingBottom;

    private boolean mIsPanelExpandedBeyondHalf;
    private boolean mWasPanelExpandedBeyondHalf;

    /**
     * @param panel             The panel.
     * @param layoutResource    The Layout that contains the Text View we're managing.
     * @param layoutId          The resource ID of the layout.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     * @param peekedDimension   The dimension resource for the padding when the Overlay is Peeked.
     * @param expandedDimension The dimension resource for the padding when the Overlay is Expanded.
     */
    public OverlayPanelRepaddingTextView(
            OverlayPanel panel,
            int layoutResource,
            int layoutId,
            Context context,
            ViewGroup container,
            DynamicResourceLoader resourceLoader,
            int peekedDimension,
            int expandedDimension) {
        super(panel, layoutResource, layoutId, context, container, resourceLoader);
        mPeekedEndButtonsWidth =
                peekedDimension == 0 ? 0 : context.getResources().getDimension(peekedDimension);
        mExpandedEndButtonsWidth =
                expandedDimension == 0 ? 0 : context.getResources().getDimension(expandedDimension);
    }

    /**
     * Updates the text view during the transition of the Overlay from Peeked to Expanded states.
     * @param percentage A value from 0 to 1 that indicates the degree to which the panel has
     *        been expanded.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        mIsPanelExpandedBeyondHalf = percentage > REPADDING_THRESHOLD;
        invalidateIfNeeded(false);
    }

    @Override
    public void invalidate() {
        invalidateIfNeeded(true);
    }

    /**
     * Invalidates the view, if needed.  Checks the {@code mIsPanelExpandedBeyondHalf} private
     * member to see if the panel is beyond a threshold that's part way between peek and expanded
     * states.
     * @param alwaysInvalidate When {@code true}, makes sure that {@link #invalidate} is called.
     */
    protected void invalidateIfNeeded(boolean alwaysInvalidate) {
        View view = getView();
        if (view == null
                || (!alwaysInvalidate
                        && mIsPanelExpandedBeyondHalf == mWasPanelExpandedBeyondHalf)) {
            return;
        }

        mWasPanelExpandedBeyondHalf = mIsPanelExpandedBeyondHalf;
        int barPaddingWidth =
                (int)
                        (mIsPanelExpandedBeyondHalf
                                ? mExpandedEndButtonsWidth
                                : mPeekedEndButtonsWidth);
        view.setPaddingRelative(mPaddingStart, mPaddingTop, barPaddingWidth, mPaddingBottom);
        super.invalidate();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        View view = getView();
        mPaddingStart = view.getPaddingStart();
        mPaddingTop = view.getPaddingTop();
        mPaddingBottom = view.getPaddingBottom();
    }
}
