// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.util.ViewUtils;

import java.util.ArrayList;

/**
 * A widget for showing a list of omnibox suggestions.
 */
@VisibleForTesting
public class OmniboxSuggestionsList extends ListView {
    private static final int OMNIBOX_RESULTS_BG_COLOR = 0xFFFFFFFF;
    private static final int OMNIBOX_INCOGNITO_RESULTS_BG_COLOR = 0xFF3C4043;

    private final OmniboxSuggestionListEmbedder mEmbedder;
    private final View mAnchorView;
    private final View mAlignmentView;

    private final int[] mTempPosition = new int[2];
    private final Rect mTempRect = new Rect();

    /**
     * Provides the capabilities required to embed the omnibox suggestion list into the UI.
     */
    public interface OmniboxSuggestionListEmbedder {
        /** Return the anchor view the suggestion list should be drawn below. */
        View getAnchorView();

        /**
         * Return the view that the omnibox suggestions should be aligned horizontally to.  The
         * view must be a descendant of {@link #getAnchorView()}.  If null, the suggestions will
         * be aligned to the start of {@link #getAnchorView()}.
         */
        @Nullable
        View getAlignmentView();

        /** Return the delegate used to interact with the Window. */
        WindowDelegate getWindowDelegate();

        /** Return whether the suggestions are being rendered in the tablet UI. */
        boolean isTablet();

        /** Return whether the current state is viewing incognito. */
        boolean isIncognito();
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     * @param embedder The embedder for the omnibox list providing access to external views and
     *                 services.
     */
    public OmniboxSuggestionsList(Context context, OmniboxSuggestionListEmbedder embedder) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        mEmbedder = embedder;
        setDivider(null);
        setFocusable(true);
        setFocusableInTouchMode(true);

        int paddingBottom = context.getResources().getDimensionPixelOffset(
                R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);

        refreshPopupBackground();

        mAnchorView = mEmbedder.getAnchorView();
        mAlignmentView = mEmbedder.getAlignmentView();
        if (mAlignmentView != null) {
            adjustSidePadding();
            mAlignmentView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    adjustSidePadding();
                }
            });
        }
    }

    private void adjustSidePadding() {
        if (mAlignmentView == null) return;

        ViewUtils.getRelativeLayoutPosition(mAnchorView, mAlignmentView, mTempPosition);
        setPadding(mTempPosition[0], getPaddingTop(),
                mAnchorView.getWidth() - mAlignmentView.getWidth() - mTempPosition[0],
                getPaddingBottom());
    }

    /**
     * Show (and properly size) the suggestions list.
     */
    void show() {
        if (getVisibility() != VISIBLE) {
            setVisibility(VISIBLE);
            if (getSelectedItemPosition() != 0) setSelection(0);
        }
    }

    /**
     * Update the suggestion popup background to reflect the current state.
     */
    void refreshPopupBackground() {
        setBackground(getSuggestionPopupBackground());
    }

    /**
     * @return The background for the omnibox suggestions popup.
     */
    private Drawable getSuggestionPopupBackground() {
        int omniboxResultsColorForNonIncognito = OMNIBOX_RESULTS_BG_COLOR;
        int omniboxResultsColorForIncognito = OMNIBOX_INCOGNITO_RESULTS_BG_COLOR;

        int color = mEmbedder.isIncognito() ? omniboxResultsColorForIncognito
                                            : omniboxResultsColorForNonIncognito;
        if (!isHardwareAccelerated()) {
            // When HW acceleration is disabled, changing mSuggestionList' items somehow erases
            // mOmniboxResultsContainer' background from the area not covered by mSuggestionList.
            // To make sure mOmniboxResultsContainer is always redrawn, we make list background
            // color slightly transparent. This makes mSuggestionList.isOpaque() to return false,
            // and forces redraw of the parent view (mOmniboxResultsContainer).
            if (Color.alpha(color) == 255) {
                color = Color.argb(254, Color.red(color), Color.green(color), Color.blue(color));
            }
        }
        return new ColorDrawable(color);
    }

    /**
     * Invalidates all of the suggestion views in the list.  Only applicable when this
     * is visible.
     */
    void invalidateSuggestionViews() {
        if (!isShown()) return;
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i) instanceof SuggestionView) {
                getChildAt(i).postInvalidateOnAnimation();
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        View contentView =
                mEmbedder.getAnchorView().getRootView().findViewById(android.R.id.content);
        ViewUtils.getRelativeLayoutPosition(contentView, mAnchorView, mTempPosition);
        int anchorY = mTempPosition[1];
        int anchorBottomRelativeToContent = anchorY + mAnchorView.getMeasuredHeight();

        mEmbedder.getWindowDelegate().getWindowVisibleDisplayFrame(mTempRect);
        int availableViewportHeight = mTempRect.height() - anchorBottomRelativeToContent;
        super.onMeasure(
                MeasureSpec.makeMeasureSpec(mAnchorView.getMeasuredWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(availableViewportHeight,
                        mEmbedder.isTablet() ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY));
    }

    @Override
    protected void layoutChildren() {
        super.layoutChildren();
        // In ICS, the selected view is not marked as selected despite calling setSelection(0),
        // so we bootstrap this after the children have been laid out.
        if (!isInTouchMode() && getSelectedView() != null) {
            getSelectedView().setSelected(true);
        }
    }

    /**
     * Update the layout direction of the suggestions.
     */
    void updateSuggestionsLayoutDirection(int layoutDirection) {
        if (!isShown()) return;

        for (int i = 0; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (!(childView instanceof SuggestionView)) continue;
            ViewCompat.setLayoutDirection(childView, layoutDirection);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Ensure none of the views are reused when re-attaching as the TextViews in the suggestions
        // do not handle it in all cases.  https://crbug.com/851839
        reclaimViews(new ArrayList<>());
    }
}
