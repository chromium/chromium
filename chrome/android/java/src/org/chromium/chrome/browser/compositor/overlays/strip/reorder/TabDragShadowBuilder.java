// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Point;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/**
 * A custom {@link View.DragShadowBuilder} that renders a high-fidelity tab drag shadow with an
 * outer drop-shadow elevation glow.
 */
@NullMarked
public class TabDragShadowBuilder extends DragShadowBuilder {
    private static final String TAG = "TabDragShadowBuilder";

    // Touch offset for drag shadow view.
    private final Point mDragShadowOffset;
    // Source initiating drag - to call updateDragShadow().
    private final View mDragSourceView;
    // Paint for the shadow.
    private final Paint mShadowPaint;
    // Corner radius for the shadow bounds.
    private final float mCornerRadius;

    // Whether drag shadow should be shown.
    private boolean mShowDragShadow;

    /**
     * Constructs a {@link TabDragShadowBuilder}.
     *
     * @param dragSourceView View initiating the drag.
     * @param shadowView View representing the drag shadow.
     * @param dragShadowOffset Touch offset for the drag shadow.
     */
    public TabDragShadowBuilder(View dragSourceView, View shadowView, Point dragShadowOffset) {
        // Store the View parameter.
        super(shadowView);
        mDragShadowOffset = dragShadowOffset;
        mDragSourceView = dragSourceView;

        // Set up the shadow paint.
        Context context = shadowView.getContext();
        Resources resources = shadowView.getResources();
        mShadowPaint = new Paint();
        mShadowPaint.setAntiAlias(true);
        mShadowPaint.setColor(context.getColor(R.color.tab_strip_reorder_shadow_color));
        float blurThickness =
                resources.getDimension(R.dimen.tab_strip_dragged_tab_shadow_thickness);
        mShadowPaint.setMaskFilter(new BlurMaskFilter(blurThickness, BlurMaskFilter.Blur.OUTER));
        mCornerRadius = resources.getDimension(R.dimen.tab_grid_card_bg_radius);
    }

    /**
     * Updates whether the drag shadow is shown and requests a redraw.
     *
     * @param show Whether the drag shadow should be visible.
     */
    public void update(boolean show) {
        mShowDragShadow = show;
        mDragSourceView.updateDragShadow(this);
    }

    @Override
    public void onDrawShadow(Canvas canvas) {
        View shadowView = assumeNonNull(getView());
        if (mShowDragShadow) {
            View cardView = shadowView.findViewById(R.id.card_view);
            if (cardView == null) {
                shadowView.draw(canvas); // Fallback
                return;
            }
            // Draw the shadow.
            canvas.drawRoundRect(
                    cardView.getLeft(),
                    cardView.getTop(),
                    cardView.getRight(),
                    cardView.getBottom(),
                    mCornerRadius,
                    mCornerRadius,
                    mShadowPaint);

            // Draw the view on top of the shadow.
            shadowView.draw(canvas);
        } else {
            // When drag shadow should hide, replace with empty ImageView.
            ImageView imageView = new ImageView(shadowView.getContext());
            imageView.layout(0, 0, shadowView.getWidth(), shadowView.getHeight());
            imageView.draw(canvas);
        }
    }

    // Defines a callback that sends the drag shadow dimensions and touch point
    // back to the system.
    @Override
    public void onProvideShadowMetrics(Point size, Point touch) {
        // Set the size parameter's width and height values. These get back to the system
        // through the size parameter.
        View shadowView = assumeNonNull(getView());
        size.set(shadowView.getWidth(), shadowView.getHeight());
        touch.set(mDragShadowOffset.x, mDragShadowOffset.y);
        Log.d(TAG, "DnD onProvideShadowMetrics: %s", mDragShadowOffset);
    }

    /**
     * Sets whether the drag shadow is shown without dispatching an immediate update.
     *
     * @param show Whether the drag shadow should be visible.
     */
    public void setShowDragShadow(boolean show) {
        mShowDragShadow = show;
    }

    /** Returns whether the drag shadow is shown. */
    public boolean getShowDragShadow() {
        return mShowDragShadow;
    }
}
