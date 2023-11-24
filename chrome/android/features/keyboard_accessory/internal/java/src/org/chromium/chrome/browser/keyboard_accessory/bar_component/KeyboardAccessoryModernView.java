// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.OvershootInterpolator;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
class KeyboardAccessoryModernView extends KeyboardAccessoryView {
    private static final int ARRIVAL_ANIMATION_DURATION_MS = 300;
    private static final float ARRIVAL_ANIMATION_BOUNCE_LENGTH_DIP = 200f;
    private static final float ARRIVAL_ANIMATION_TENSION = 1f;

    private Callback<Integer> mObfuscatedLastChildAt;
    private ObjectAnimator mAnimator;
    private float mLastBarItemsViewPosition;

    // Records the first time a user scrolled to suppress an IPH explaining how scrolling works.
    private final RecyclerView.OnScrollListener mScrollingIphCallback =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
                    if (newState != RecyclerView.SCROLL_STATE_IDLE) {
                        mBarItemsView.removeOnScrollListener(mScrollingIphCallback);
                        KeyboardAccessoryIPHUtils.emitScrollingEvent();
                    }
                }
            };

    /**
     * This decoration ensures that the last item is right-aligned.
     * To do this, it subtracts the widths, margins and offsets of all items in the recycler view
     * from the RecyclerView's total width. If the items fill the whole recycler view, the last item
     * uses the same offset as all other items.
     */
    private class StickyLastItemDecoration extends HorizontalDividerItemDecoration {
        StickyLastItemDecoration(@Px int minimalLeadingHorizontalMargin) {
            super(minimalLeadingHorizontalMargin);
        }

        @Override
        protected int getItemOffsetInternal(
                final View view, final RecyclerView parent, RecyclerView.State state) {
            int minimalOffset = super.getItemOffsetInternal(view, parent, state);
            if (!isLastItem(parent, view, parent.getAdapter().getItemCount())) return minimalOffset;
            if (view.getWidth() == 0 && state.didStructureChange()) {
                // When the RecyclerView is first created, its children aren't measured yet and miss
                // dimensions. Therefore, estimate the offset and recalculate after UI has loaded.
                view.post(parent::invalidateItemDecorations);
                return parent.getWidth() - estimateLastElementWidth(view);
            }
            return Math.max(getSpaceLeftInParent(parent), minimalOffset);
        }

        private int getSpaceLeftInParent(RecyclerView parent) {
            int spaceLeftInParent = parent.getWidth();
            spaceLeftInParent -= getOccupiedSpaceByChildren(parent);
            spaceLeftInParent -= getOccupiedSpaceByChildrenOffsets(parent);
            spaceLeftInParent -= parent.getPaddingEnd() + parent.getPaddingStart();
            return spaceLeftInParent;
        }

        private int estimateLastElementWidth(View view) {
            assert view instanceof ViewGroup;
            return ((ViewGroup) view).getChildCount()
                    * getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.keyboard_accessory_tab_size);
        }

        private int getOccupiedSpaceByChildren(RecyclerView parent) {
            int occupiedSpace = 0;
            for (int i = 0; i < parent.getChildCount(); i++) {
                occupiedSpace += getOccupiedSpaceForView(parent.getChildAt(i));
            }
            return occupiedSpace;
        }

        private int getOccupiedSpaceForView(View view) {
            int occupiedSpace = view.getWidth();
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            if (lp instanceof MarginLayoutParams) {
                occupiedSpace += ((MarginLayoutParams) lp).leftMargin;
                occupiedSpace += ((MarginLayoutParams) lp).rightMargin;
            }
            return occupiedSpace;
        }

        private int getOccupiedSpaceByChildrenOffsets(RecyclerView parent) {
            return (parent.getChildCount() - 1) * super.getItemOffsetInternal(null, null, null);
        }

        private boolean isLastItem(RecyclerView parent, View view, int itemCount) {
            return parent.getChildAdapterPosition(view) == itemCount - 1;
        }
    }

    /** Constructor for inflating from XML. */
    public KeyboardAccessoryModernView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        int pad = getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding);
        // Ensure the last element (although scrollable) is always end-aligned.
        mBarItemsView.addItemDecoration(new StickyLastItemDecoration(pad));
        mBarItemsView.addOnScrollListener(mScrollingIphCallback);

        // Remove any paddings that might be inherited since this messes up the fading edge.
        ViewCompat.setPaddingRelative(mBarItemsView, 0, 0, 0, 0);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        // Request update for the offset of the icons at the end of the accessory bar:
        mBarItemsView.post(mBarItemsView::invalidateItemDecorations);
    }

    @Override
    void setVisible(boolean visible) {
        TraceEvent.begin("KeyboardAccessoryModernView#setVisible");
        super.setVisible(visible);
        if (visible) {
            mBarItemsView.post(mBarItemsView::invalidateItemDecorations);
            // Animate the suggestions only if the bar wasn't visible already.
            if (getVisibility() != View.VISIBLE) animateSuggestionArrival();
        }
        TraceEvent.end("KeyboardAccessoryModernView#setVisible");
    }

    @Override
    protected void onItemsChanged() {
        super.onItemsChanged();
        if (isLastChildObfuscated()) {
            mObfuscatedLastChildAt.onResult(mBarItemsView.indexOfChild(getLastChild()));
        }
    }

    ViewRectProvider getSwipingIphRect() {
        View lastChild = getLastChild();
        if (lastChild == null) return null;
        ViewRectProvider provider = new ViewRectProvider(getLastChild());
        provider.setIncludePadding(true);
        return provider;
    }

    private boolean isLastChildObfuscated() {
        View lastChild = getLastChild();
        RecyclerView.Adapter adapter = mBarItemsView.getAdapter();
        // The recycler view isn't ready yet, so no children can be considered:
        if (lastChild == null || adapter == null) return false;
        // The last child wasn't even rendered, so it's definitely not visible:
        if (mBarItemsView.indexOfChild(lastChild) < adapter.getItemCount()) return true;
        // The last child is partly off screen:
        return getLayoutDirection() == LAYOUT_DIRECTION_RTL
                ? lastChild.getX() < 0
                : lastChild.getX() + lastChild.getWidth() > mBarItemsView.getWidth();
    }

    private View getLastChild() {
        for (int i = mBarItemsView.getChildCount() - 1; i >= 0; --i) {
            View lastChild = mBarItemsView.getChildAt(i);
            if (lastChild == null) continue;
            return lastChild;
        }
        return null;
    }

    void setObfuscatedLastChildAt(Callback<Integer> obfuscatedLastChildAt) {
        mObfuscatedLastChildAt = obfuscatedLastChildAt;
    }

    void setAccessibilityMessage(boolean hasSuggestions) {
        setContentDescription(
                getContext()
                        .getString(
                                hasSuggestions
                                        ? R.string
                                                .autofill_keyboard_accessory_modern_content_description
                                        : R.string
                                                .autofill_keyboard_accessory_modern_content_fallback_description));
    }

    private void animateSuggestionArrival() {
        if (areAnimationsDisabled()) return;
        int bounceDirection = getLayoutDirection() == LAYOUT_DIRECTION_RTL ? 1 : -1;
        if (mAnimator != null && mAnimator.isRunning()) {
            mAnimator.cancel();
        } else {
            mLastBarItemsViewPosition = mBarItemsView.getX();
        }

        float start =
                mLastBarItemsViewPosition
                        - bounceDirection
                                * ARRIVAL_ANIMATION_BOUNCE_LENGTH_DIP
                                * getContext().getResources().getDisplayMetrics().density;
        mBarItemsView.setTranslationX(start);
        mAnimator =
                ObjectAnimator.ofFloat(
                        mBarItemsView, "translationX", start, mLastBarItemsViewPosition);
        mAnimator.setDuration(ARRIVAL_ANIMATION_DURATION_MS);
        mAnimator.setInterpolator(new OvershootInterpolator(ARRIVAL_ANIMATION_TENSION));
        mAnimator.start();
    }
}
