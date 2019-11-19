// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.support.v4.view.ViewCompat;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.chrome.browser.util.ViewUtils;

import java.util.ArrayList;

/**
 * A widget for showing a list of omnibox suggestions.
 */
@VisibleForTesting
public class OmniboxSuggestionsList extends ListView {
    private final int[] mTempPosition = new int[2];
    private final Rect mTempRect = new Rect();
    private final int mStandardBgColor;
    private final int mIncognitoBgColor;

    private OmniboxSuggestionListEmbedder mEmbedder;
    private View mAnchorView;
    private View mAlignmentView;
    private OnGlobalLayoutListener mAnchorViewLayoutListener;
    private OnLayoutChangeListener mAlignmentViewLayoutListener;

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsList(Context context) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        setDivider(null);
        setFocusable(true);
        setFocusableInTouchMode(true);

        final Resources resources = context.getResources();
        mStandardBgColor = ChromeColors.getDefaultThemeColor(resources, false);
        mIncognitoBgColor = ChromeColors.getDefaultThemeColor(resources, true);
        int paddingBottom =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);
    }

    /** Set the embedder for the list view. */
    void setEmbedder(OmniboxSuggestionListEmbedder embedder) {
        assert mEmbedder == null;
        mEmbedder = embedder;
        mAnchorView = mEmbedder.getAnchorView();
        // Prior to Android M, the contextual actions associated with the omnibox were anchored to
        // the top of the screen and not a floating copy/paste menu like on newer versions.  As a
        // result of this, the toolbar is pushed down in these Android versions and we need to
        // montior those changes to update the positioning of the list.
        mAnchorViewLayoutListener = new OnGlobalLayoutListener() {
            private int mOffsetInWindow;

            @Override
            public void onGlobalLayout() {
                int offsetInWindow = 0;
                View currentView = mAnchorView;
                while (true) {
                    offsetInWindow += currentView.getTop();
                    ViewParent parent = currentView.getParent();
                    if (parent == null || !(parent instanceof View)) break;
                    currentView = (View) parent;
                }
                if (mOffsetInWindow == offsetInWindow) return;
                mOffsetInWindow = offsetInWindow;
                requestLayout();
            }
        };

        mAlignmentView = mEmbedder.getAlignmentView();
        if (mAlignmentView != null) {
            mAlignmentViewLayoutListener = new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    adjustSidePadding();
                }
            };
        } else {
            mAlignmentViewLayoutListener = null;
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
    void refreshPopupBackground(boolean isIncognito) {
        int color = isIncognito ? mIncognitoBgColor : mStandardBgColor;
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
        setBackground(new ColorDrawable(color));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        View contentView =
                mEmbedder.getAnchorView().getRootView().findViewById(android.R.id.content);
        ViewUtils.getRelativeLayoutPosition(contentView, mAnchorView, mTempPosition);
        int anchorY = mTempPosition[1];
        int anchorBottomRelativeToContent = anchorY + mAnchorView.getMeasuredHeight();

        // Update the layout params to ensure the parent correctly positions the suggestions under
        // the anchor view.
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        if (layoutParams != null && layoutParams instanceof MarginLayoutParams) {
            ((MarginLayoutParams) layoutParams).topMargin = anchorBottomRelativeToContent;
        }
        mEmbedder.getWindowDelegate().getWindowVisibleDisplayFrame(mTempRect);
        int availableViewportHeight = mTempRect.height() - anchorBottomRelativeToContent;
        super.onMeasure(
                MeasureSpec.makeMeasureSpec(mAnchorView.getMeasuredWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(availableViewportHeight,
                        mEmbedder.isTablet() ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY));
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        int selectedPosition = getSelectedItemPosition();
        int itemCount = getAdapter().getCount();
        if (KeyNavigationUtil.isGoDown(event)) {
            if (selectedPosition >= itemCount - 1) {
                // Do not pass down events when the last item is already selected as it will
                // dismiss the suggestion list.
                return true;
            }

            if (selectedPosition == ListView.INVALID_POSITION) {
                // When clearing the selection after a text change, state is not reset
                // correctly so hitting down again will cause it to start from the previous
                // selection point. We still have to send the key down event to let the list
                // view items take focus, but then we select the first item explicitly.
                boolean result = super.onKeyDown(keyCode, event);
                setSelection(0);
                return result;
            }
        } else if (KeyNavigationUtil.isGoRight(event)
                && selectedPosition != ListView.INVALID_POSITION) {
            View selectedView = getSelectedView();
            if (selectedView != null) return selectedView.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isEnter(event)
                && selectedPosition != ListView.INVALID_POSITION) {
            View selectedView = getSelectedView();
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
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

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mAnchorView.getViewTreeObserver().addOnGlobalLayoutListener(mAnchorViewLayoutListener);
        if (mAlignmentView != null) {
            adjustSidePadding();
            mAlignmentView.addOnLayoutChangeListener(mAlignmentViewLayoutListener);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Ensure none of the views are reused when re-attaching as the TextViews in the suggestions
        // do not handle it in all cases.  https://crbug.com/851839
        reclaimViews(new ArrayList<>());
        mAnchorView.getViewTreeObserver().removeOnGlobalLayoutListener(mAnchorViewLayoutListener);
        if (mAlignmentView != null) {
            mAlignmentView.removeOnLayoutChangeListener(mAlignmentViewLayoutListener);
        }
    }
}
