// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/**
 * Abstract base {@link RecyclerView.ItemDecoration} that provides shared state, lifecycle, view
 * resolution, and rendering template for vertical tab drop indicator decorations.
 */
@NullMarked
public abstract class BaseVerticalTabDropIndicatorDecoration extends RecyclerView.ItemDecoration {
    protected final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    protected final RectF mRectF = new RectF();
    protected final int mIndicatorThickness;

    private @Nullable DropTargetResult mDropTargetResult;

    /**
     * @param context The {@link Context} used to retrieve dimension resources.
     */
    public BaseVerticalTabDropIndicatorDecoration(Context context) {
        Resources res = context.getResources();
        mIndicatorThickness = res.getDimensionPixelSize(R.dimen.vertical_tab_spine_width);
        mPaint.setStyle(Paint.Style.FILL);
    }

    /**
     * Updates the current drop target result.
     *
     * @param dropTargetResult The computed {@link DropTargetResult}, or null to hide the indicator.
     */
    public void setDropTargetResult(@Nullable DropTargetResult dropTargetResult) {
        mDropTargetResult = dropTargetResult;
    }

    /** Returns the current {@link DropTargetResult}, or null if cleared or inactive. */
    public @Nullable DropTargetResult getDropTargetResult() {
        return mDropTargetResult;
    }

    /** Clears the current drop target result, dismissing the drop indicator. */
    public void clear() {
        mDropTargetResult = null;
    }

    @Override
    public final void onDrawOver(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (mDropTargetResult == null || !shouldDraw(mDropTargetResult)) return;

        if (!calculateBounds(mRectF, parent, mDropTargetResult)) return;

        mPaint.setColor(SemanticColorUtils.getColorPrimary(parent.getContext()));
        float cornerRadius = mIndicatorThickness / 2.0f;
        c.drawRoundRect(mRectF, cornerRadius, cornerRadius, mPaint);
    }

    /**
     * Determines whether the indicator should be drawn for the given drop target result.
     *
     * @param result The {@link DropTargetResult} to evaluate.
     * @return True if drawing should proceed, false otherwise.
     */
    protected abstract boolean shouldDraw(DropTargetResult result);

    /**
     * Calculates the bounding rectangle for the drop indicator.
     *
     * @param outRect The {@link RectF} to populate with bounds.
     * @param parent The {@link RecyclerView} hosting the decoration.
     * @param result The {@link DropTargetResult} to calculate bounds for.
     * @return True if valid bounds were calculated and drawing should proceed, false otherwise.
     */
    protected abstract boolean calculateBounds(
            RectF outRect, RecyclerView parent, DropTargetResult result);

    /**
     * Resolves the target view from the drop target result if attached to the parent {@link
     * RecyclerView}.
     *
     * @param result The {@link DropTargetResult} containing the target view holder.
     * @param parent The {@link RecyclerView} parent.
     * @return The item {@link View} if attached to parent, or null otherwise.
     */
    protected @Nullable View getAttachedTargetView(DropTargetResult result, RecyclerView parent) {
        if (result.targetViewHolder != null
                && result.targetViewHolder.itemView.getParent() == parent) {
            return result.targetViewHolder.itemView;
        }
        return null;
    }

    Paint getPaintForTesting() {
        return mPaint;
    }

    RectF getRectFForTesting() {
        return mRectF;
    }
}
