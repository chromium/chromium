// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.os.Build;
import android.support.v4.view.ViewCompat;
import android.support.v7.view.ContextThemeWrapper;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Interpolator;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;
import org.chromium.chrome.browser.ntp.cards.ScrollToLoadListener;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Simple wrapper on top of a RecyclerView that will acquire focus when tapped.  Ensures the
 * New Tab page receives focus when clicked.
 */
public class SuggestionsRecyclerView extends RecyclerView {
    private static final Interpolator DISMISS_INTERPOLATOR =
            Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR;
    private static final int DISMISS_ANIMATION_TIME_MS = 300;

    private final GestureDetector mGestureDetector;
    private final LinearLayoutManager mLayoutManager;

    // The ScrollToLoadListener triggers loading more content when the user is near the end.
    @Nullable private ScrollToLoadListener mScrollToLoadListener;

    /**
     * Total height of the items being dismissed.  Tracked to allow the bottom space to compensate
     * for their removal animation and avoid moving the scroll position.
     */
    private int mCompensationHeight;

    /**
     * Height compensation value for each item being dismissed. Since dismissals sometimes include
     * sibling elements, and these don't get the standard treatment, we track the total height
     * associated with the element the user interacted with.
     */
    private final Map<ViewHolder, Integer> mCompensationHeightMap = new HashMap<>();

    /**
     * Whether the {@link SuggestionsRecyclerView} and its children should react to touch events.
     */
    private boolean mTouchEnabled = true;

    /** The ui config for this view. */
    private UiConfig mUiConfig;

    private Runnable mCloseContextMenuCallback;

    public SuggestionsRecyclerView(Context context) {
        this(context, null);
    }

    public SuggestionsRecyclerView(Context context, AttributeSet attrs) {
        this(context, attrs, new LinearLayoutManager(context));
    }

    @SuppressWarnings("RestrictTo")
    protected SuggestionsRecyclerView(
            Context context, AttributeSet attrs, LinearLayoutManager layoutManager) {
        super(new ContextThemeWrapper(context, R.style.NewTabPageRecyclerView), attrs);

        Resources res = getContext().getResources();
        setBackgroundColor(SuggestionsConfig.getBackgroundColor(res));
        setLayoutParams(new LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setFocusable(true);
        setFocusableInTouchMode(true);
        setContentDescription(res.getString(R.string.accessibility_new_tab_page));
        setClipToPadding(false);

        mGestureDetector =
                new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        boolean retVal = super.onSingleTapUp(e);
                        requestFocus();
                        return retVal;
                    }
                });
        mLayoutManager = layoutManager;
        setLayoutManager(layoutManager);
        setHasFixedSize(true);

        ItemTouchHelper helper = new ItemTouchHelper(new ItemTouchCallbacks());
        helper.attachToRecyclerView(this);

        addOnScrollListener(new SuggestionsMetrics.ScrollEventReporter());

        // Work around https://crbug.com/943873 where default focus highlight shows up after
        // toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            setDefaultFocusHighlightEnabled(false);
        }
    }

    public boolean isFirstItemVisible() {
        return mLayoutManager.findFirstVisibleItemPosition() == 0;
    }

    /**
     * Sets whether the {@link SuggestionsRecyclerView} and its children should react to touch
     * events.
     */
    public void setTouchEnabled(boolean enabled) {
        mTouchEnabled = enabled;
    }

    /**
     * Returns the approximate adapter position that the user has scrolled to. The purpose of this
     * value is that it can be stored and later retrieved to restore a scroll position that is
     * familiar to the user, showing (part of) the same content the user was previously looking at.
     * This position is valid for that purpose regardless of device orientation changes. Note that
     * if the underlying data has changed in the meantime, different content would be shown for this
     * position.
     */
    public int getScrollPosition() {
        return getLinearLayoutManager().findFirstVisibleItemPosition();
    }

    /**
     * @return Whether the {@link SuggestionsRecyclerView} and its children should react to touch
     * events.
     */
    protected boolean getTouchEnabled() {
        return mTouchEnabled;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        mGestureDetector.onTouchEvent(ev);
        if (!getTouchEnabled()) return true;
        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_UP
                || ev.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            setLayoutFrozen(false);
        }
        return super.dispatchTouchEvent(ev);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (!getTouchEnabled()) return false;

        // Action down would already have been handled in onInterceptTouchEvent
        if (ev.getActionMasked() != MotionEvent.ACTION_DOWN) {
            mGestureDetector.onTouchEvent(ev);
        }
        return super.onTouchEvent(ev);
    }

    @Override
    public void focusableViewAvailable(View v) {
        // To avoid odd jumps during NTP animation transitions, we do not attempt to give focus
        // to child views if this scroll view already has focus.
        if (hasFocus()) return;
        super.focusableViewAvailable(v);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Fixes landscape transitions when unfocusing the URL bar: crbug.com/288546
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return super.onCreateInputConnection(outAttrs);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // When the viewport configuration changes, we want to update the display style so that the
        // observers are aware of the new available space. Another moment to do this update could
        // be through a OnLayoutChangeListener, but then we get notified of the change after the
        // layout pass, which means that the new style will only be visible after layout happens
        // again. We prefer updating here to avoid having to require that additional layout pass.
        if (mUiConfig != null) mUiConfig.updateDisplayStyle();

        // Close the Context Menu as it may have moved (https://crbug.com/642688).
        if (mCloseContextMenuCallback != null) mCloseContextMenuCallback.run();
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int numberViews = getChildCount();
        for (int i = 0; i < numberViews; ++i) {
            View view = getChildAt(i);
            ((NewTabPageViewHolder) getChildViewHolder(view)).updateLayoutParams();
        }
        super.onLayout(changed, l, t, r, b);
    }

    public void init(UiConfig uiConfig, Runnable closeContextMenuCallback) {
        mUiConfig = uiConfig;
        mCloseContextMenuCallback = closeContextMenuCallback;
    }

    public NewTabPageAdapter getNewTabPageAdapter() {
        return (NewTabPageAdapter) getAdapter();
    }

    public LinearLayoutManager getLinearLayoutManager() {
        return mLayoutManager;
    }

    /** Called when an item is in the process of being removed from the view. */
    public void onItemDismissStarted(ViewHolder viewHolder) {
        assert !mCompensationHeightMap.containsKey(viewHolder);

        int dismissedHeight = 0;
        List<ViewHolder> siblings = getDismissalGroupViewHolders(viewHolder);
        for (ViewHolder siblingViewHolder : siblings) {
            dismissedHeight += siblingViewHolder.itemView.getHeight();
        }

        mCompensationHeightMap.put(viewHolder, dismissedHeight);
        mCompensationHeight += dismissedHeight;
    }

    /** Called when an item has finished being removed from the view. */
    public void onItemDismissFinished(ViewHolder viewHolder) {
        if (!mCompensationHeightMap.containsKey(viewHolder)) return;

        mCompensationHeight -= mCompensationHeightMap.remove(viewHolder);

        assert mCompensationHeight >= 0;
    }

    /**
     * Animates the card being swiped to the right as if the user had dismissed it. Any changes to
     * the animation here should be reflected also in {@link #updateViewStateForDismiss} and reset
     * in {@link CardViewHolder#onBindViewHolder()}.
     */
    public void dismissItemWithAnimation(final ViewHolder viewHolder) {
        List<ViewHolder> siblings = getDismissalGroupViewHolders(viewHolder);
        if (siblings.isEmpty()) return;

        List<Animator> animations = new ArrayList<>();
        for (ViewHolder dismissSibling : siblings) {
            addDismissalAnimators(animations, dismissSibling.itemView);
        }

        AnimatorSet animation = new AnimatorSet();
        animation.playTogether(animations);
        animation.setDuration(DISMISS_ANIMATION_TIME_MS);
        animation.setInterpolator(DISMISS_INTERPOLATOR);
        animation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                SuggestionsRecyclerView.this.onItemDismissStarted(viewHolder);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // It is possible that by the time the animation ends, we navigated away from the
                // container and it got destroyed. In that case, abort. (https://crbug.com/668945)
                if (!ViewCompat.isAttachedToWindow(viewHolder.itemView)) return;

                dismissItemInternal(viewHolder);
                SuggestionsRecyclerView.this.onItemDismissFinished(viewHolder);
            }
        });
        animation.start();
    }

    private void dismissItemInternal(ViewHolder viewHolder) {
        // Re-check the position in case the adapter has changed.
        final int position = viewHolder.getAdapterPosition();
        if (position == RecyclerView.NO_POSITION) {
            // The item does not exist anymore, so ignore.
            return;
        }
        getNewTabPageAdapter().dismissItem(position, removedItemTitle -> {
            announceForAccessibility(getResources().getString(
                    R.string.ntp_accessibility_item_removed, removedItemTitle));
            if (mScrollToLoadListener != null) mScrollToLoadListener.onItemDismissed();
        });
    }

    /**
     * @param animations in/out list holding the animators to play.
     * @param view  view to animate.
     */
    private void addDismissalAnimators(List<Animator> animations, View view) {
        animations.add(ObjectAnimator.ofFloat(view, View.ALPHA, 0f));
        animations.add(ObjectAnimator.ofFloat(view, View.TRANSLATION_X, (float) view.getWidth()));
    }

    /**
     * Update the view's state as it is being swiped away. Any changes to the animation here should
     * be reflected also in {@link #dismissItemWithAnimation(ViewHolder)} and reset in
     * {@link CardViewHolder#onBindViewHolder()}.
     * @param dX The amount of horizontal displacement caused by user's action.
     * @param viewHolder The view holder containing the view to be updated.
     */
    private void updateViewStateForDismiss(float dX, ViewHolder viewHolder) {
        viewHolder.itemView.setTranslationX(dX);

        float input = Math.abs(dX) / viewHolder.itemView.getMeasuredWidth();
        float alpha = 1 - DISMISS_INTERPOLATOR.getInterpolation(input);
        viewHolder.itemView.setAlpha(alpha);
    }

    /**
     * Resets a card's properties affected by swipe to dismiss. Intended to be used as
     * {@link NewTabPageViewHolder.PartialBindCallback}
     */
    public static void resetForDismissCallback(NewTabPageViewHolder holder) {
        ((CardViewHolder) holder).getRecyclerView().updateViewStateForDismiss(0, holder);
    }

    /**
     * Sets the ScrollToLoadListener for the RecyclerView.
     */
    public void setScrollToLoadListener(@Nullable ScrollToLoadListener scrollToLoadListener) {
        mScrollToLoadListener = scrollToLoadListener;
        addOnScrollListener(mScrollToLoadListener);
    }

    /**
     * Clears the currently registered ScrollToLoadListener.
     */
    public void clearScrollToLoadListener() {
        if (mScrollToLoadListener == null) return;

        removeOnScrollListener(mScrollToLoadListener);
        mScrollToLoadListener = null;
    }

    private class ItemTouchCallbacks extends ItemTouchHelper.Callback {
        @Override
        public void onSwiped(ViewHolder viewHolder, int direction) {
            onItemDismissStarted(viewHolder);
            SuggestionsMetrics.recordCardSwipedAway();
            dismissItemInternal(viewHolder);
        }

        @Override
        public void clearView(RecyclerView recyclerView, ViewHolder viewHolder) {
            // clearView() is called when an interaction with the item is finished, which does
            // not mean that the user went all the way and dismissed the item before releasing it.
            // We need to check that the item has been removed.
            if (viewHolder.getAdapterPosition() == RecyclerView.NO_POSITION) {
                onItemDismissFinished(viewHolder);
            }

            super.clearView(recyclerView, viewHolder);
        }

        @Override
        public boolean onMove(RecyclerView recyclerView, ViewHolder viewHolder, ViewHolder target) {
            assert false; // Drag and drop not supported, the method will never be called.
            return false;
        }

        @Override
        public int getMovementFlags(RecyclerView recyclerView, ViewHolder viewHolder) {
            int swipeFlags = 0;
            if (((NewTabPageViewHolder) viewHolder).isDismissable()) {
                swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
            }

            return makeMovementFlags(0 /* dragFlags */, swipeFlags);
        }

        @Override
        public void onChildDraw(Canvas c, RecyclerView recyclerView, ViewHolder viewHolder,
                float dX, float dY, int actionState, boolean isCurrentlyActive) {
            // In some cases a removed child may call this method when unrelated items are
            // interacted with (https://crbug.com/664466, b/32900699), but in that case
            // getSiblingDismissalViewHolders() below will return an empty list.

            // We use our own implementation of the dismissal animation, so we don't call the
            // parent implementation. (by default it changes the translation-X and elevation)
            for (ViewHolder siblingViewHolder : getDismissalGroupViewHolders(viewHolder)) {
                updateViewStateForDismiss(dX, siblingViewHolder);
            }
        }
    }

    private List<ViewHolder> getDismissalGroupViewHolders(ViewHolder viewHolder) {
        int position = viewHolder.getAdapterPosition();
        if (position == NO_POSITION) return Collections.emptyList();

        List<ViewHolder> viewHolders = new ArrayList<>();
        Set<Integer> dismissalRange = getNewTabPageAdapter().getItemDismissalGroup(position);
        for (int i : dismissalRange) {
            ViewHolder siblingViewHolder = findViewHolderForAdapterPosition(i);
            if (siblingViewHolder == null) continue;

            viewHolders.add(siblingViewHolder);
        }
        return viewHolders;
    }
}
