// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Provides an {@link OverlayPanelInflater} that adjusts a text view to override the RTL/LTR
 * ordering when the initial text fragment is short.
 * Details in this issue: crbug.com/651389.
 */
public abstract class OverlayPanelTextViewInflater
        extends OverlayPanelRepaddingTextView implements OnLayoutChangeListener {
    private static final float SHORTNESS_FACTOR = 0.5f;

    private boolean mDidAdjustViewDirection;

    /**
     * Constructs an instance similar to an {@link OverlayPanelRepaddingTextView} that can adjust
     * the RTL/LTR ordering of text fragments whose initial values are considered short relative to
     * the width of the view.
     * @param panel             The panel.
     * @param layoutId          The resource ID of the layout.
     * @param viewId            The resource ID of the text view.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     * @param peekedDimension   The dimension resource for the padding when the Overlay is Peeked.
     * @param expandedDimension The dimension resource for the padding when the Overlay is Expanded.
     */
    public OverlayPanelTextViewInflater(OverlayPanel panel, int layoutId, int viewId,
            Context context, ViewGroup container, DynamicResourceLoader resourceLoader,
            int peekedDimension, int expandedDimension) {
        super(panel, layoutId, viewId, context, container, resourceLoader, peekedDimension,
                expandedDimension);
    }

    /**
     * Constructs an instance similar to an {@link OverlayPanelRepaddingTextView} that can adjust
     * the RTL/LTR ordering of text fragments whose initial values are considered short relative to
     * the width of the view.
     * @param panel             The panel.
     * @param layoutId          The resource ID of the layout.
     * @param viewId            The resource ID of the text view.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public OverlayPanelTextViewInflater(OverlayPanel panel, int layoutId, int viewId,
            Context context, ViewGroup container, DynamicResourceLoader resourceLoader) {
        super(panel, layoutId, viewId, context, container, resourceLoader, 0, 0);
    }

    /**
     * Subclasses must override to return the {@link TextView} once it's inflated.
     * @return The {@link TextView} or {@code null} if not yet inflated.
     */
    protected abstract TextView getTextView();

    //========================================================================================
    // OverlayPanelInflater overrides
    //========================================================================================

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        view.addOnLayoutChangeListener(this);
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        TextView textView = getTextView();
        if (!mDidAdjustViewDirection && textView != null) {
            // We only adjust the view once, based on the initial value set at layout time.
            mDidAdjustViewDirection = true;
            adjustViewDirection(textView);
        }
    }

    //========================================================================================
    // Private methods
    //========================================================================================

    /**
     * Adjusts the given {@code TextView} to have a layout direction that matches the UI direction
     * when the contents of the view is considered short (based on SHORTNESS_FACTOR).
     * @param textView The text view to adjust.
     */
    @SuppressLint("RtlHardcoded")
    private void adjustViewDirection(TextView textView) {
        float textWidth = textView.getPaint().measureText(textView.getText().toString());
        if (textWidth < SHORTNESS_FACTOR * textView.getWidth()) {
            textView.setGravity(LocalizationUtils.isLayoutRtl() ? Gravity.RIGHT : Gravity.LEFT);
        }
    }
}
