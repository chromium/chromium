// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.accessibility.AccessibilityEvent;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
class KeyboardAccessoryView extends LinearLayout {
    private static final int ARRIVAL_ANIMATION_DURATION_MS = 300;
    private static final float ARRIVAL_ANIMATION_BOUNCE_LENGTH_DIP = 200f;
    private static final float ARRIVAL_ANIMATION_TENSION = 1f;
    private static final int FADE_ANIMATION_DURATION_MS = 150; // Total duration of show/hide.
    private static final int HIDING_ANIMATION_DELAY_MS = 50; // Shortens animation duration.

    private Tracker mFeatureEngagementTracker;
    private Callback<Integer> mObfuscatedLastChildAt;
    private Callback<Boolean> mOnTouchEvent;
    private ObjectAnimator mAnimator;
    private AnimationListener mAnimationListener;
    private ViewPropertyAnimator mRunningAnimation;
    private float mLastBarItemsViewPosition;
    private boolean mShouldSkipClosingAnimation;
    private boolean mDisableAnimations;
    private boolean mAllowClicksWhileObscured;

    protected RecyclerView mBarItemsView;

    /** Interface that allows to react to animations. */
    interface AnimationListener {
        /**
         * Called if the accessory bar stopped fading in. The fade-in only happens sometimes, e.g.
         * if the bar is already visible or animations are disabled, this signal is not issued.
         */
        void onFadeInEnd();
    }

    // Records the first time a user scrolled to suppress an IPH explaining how scrolling works.
    private final RecyclerView.OnScrollListener mScrollingIphCallback =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState) {
                    if (newState != RecyclerView.SCROLL_STATE_IDLE) {
                        mBarItemsView.removeOnScrollListener(mScrollingIphCallback);
                        KeyboardAccessoryIPHUtils.emitScrollingEvent(mFeatureEngagementTracker);
                    }
                }
            };

    /**
     * This decoration ensures that the last item is right-aligned. To do this, it subtracts the
     * widths, margins and offsets of all items in the recycler view from the RecyclerView's total
     * width. If the items fill the whole recycler view, the last item uses the same offset as all
     * other items.
     */
    private class StickyLastItemDecoration extends RecyclerView.ItemDecoration {
        private final int mHorizontalMargin;

        StickyLastItemDecoration(@Px int minimalLeadingHorizontalMargin) {
            this.mHorizontalMargin = minimalLeadingHorizontalMargin;
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            if (isLayoutRtl()) {
                outRect.right = getItemOffsetInternal(view, parent, state);
            } else {
                outRect.left = getItemOffsetInternal(view, parent, state);
            }
        }

        private int getItemOffsetInternal(
                final View view, final RecyclerView parent, RecyclerView.State state) {
            if (!isLastItem(parent, view, parent.getAdapter().getItemCount())) {
                return mHorizontalMargin;
            }
            if (view.getWidth() == 0 && state.didStructureChange()) {
                // When the RecyclerView is first created, its children aren't measured yet and miss
                // dimensions. Therefore, estimate the offset and recalculate after UI has loaded.
                view.post(parent::invalidateItemDecorations);
                return parent.getWidth() - estimateLastElementWidth(view);
            }
            return Math.max(getSpaceLeftInParent(parent), mHorizontalMargin);
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
            return (parent.getChildCount() - 1) * mHorizontalMargin;
        }

        private boolean isLastItem(RecyclerView parent, View view, int itemCount) {
            return parent.getChildAdapterPosition(view) == itemCount - 1;
        }
    }

    /** Constructor for inflating from XML. */
    public KeyboardAccessoryView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent motionEvent) {
        return true; // Accessory view is a sink for all events. Touch/Click is handled earlier.
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        final boolean isViewObscured =
                (event.getFlags()
                                & (MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED
                                        | MotionEvent.FLAG_WINDOW_IS_OBSCURED))
                        != 0;
        // The event is filtered out when the keyboard accessory view is fully or partially obscured
        // given that no user education bubbles are shown to the user.
        final boolean shouldFilterEvent = isViewObscured && !mAllowClicksWhileObscured;
        mOnTouchEvent.onResult(shouldFilterEvent);

        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID)) {
            return super.onInterceptTouchEvent(event);
        }
        if (shouldFilterEvent) {
            return true;
        }
        // When keyboard accessory view is fully or partially obsured, clicks are allowed only if
        // the user education bubble is being displayed. After the first click
        // (MotionEvent.ACTION_UP), such motion events start to get filtered again. Please note that
        // a user click produces 2 motion events: MotionEvent.ACTION_DOWN and then
        // MotionEvent.ACTION_UP.
        if (event.getAction() == MotionEvent.ACTION_UP) {
            mAllowClicksWhileObscured = false;
        }

        return super.onInterceptTouchEvent(event);
    }

    @Override
    protected void onFinishInflate() {
        TraceEvent.begin("KeyboardAccessoryView#onFinishInflate");
        super.onFinishInflate();
        sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);

        mBarItemsView = findViewById(R.id.bar_items_view);
        initializeHorizontalRecyclerView(mBarItemsView);

        // Apply RTL layout changes to the view's children:
        int layoutDirection = isLayoutRtl() ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR;
        findViewById(R.id.accessory_bar_contents).setLayoutDirection(layoutDirection);
        mBarItemsView.setLayoutDirection(layoutDirection);

        // Set listener's to touch/click events so they are not propagated to the page below.
        setOnTouchListener(
                (view, motionEvent) -> {
                    performClick(); // Setting a touch listener requires this call which is a NoOp.
                    // Return that the motionEvent was consumed and needs no further handling.
                    return true;
                });
        setOnClickListener(view -> {});
        setClickable(false); // Disables the "Double-tap to activate" Talkback reading.
        setSoundEffectsEnabled(false);

        int pad = getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding);
        // Ensure the last element (although scrollable) is always end-aligned.
        mBarItemsView.addItemDecoration(new StickyLastItemDecoration(pad));
        mBarItemsView.addOnScrollListener(mScrollingIphCallback);

        // Remove any paddings that might be inherited since this messes up the fading edge.
        mBarItemsView.setPaddingRelative(0, 0, 0, 0);
        TraceEvent.end("KeyboardAccessoryView#onFinishInflate");
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        // Request update for the offset of the icons at the end of the accessory bar:
        mBarItemsView.post(mBarItemsView::invalidateItemDecorations);
    }

    void setFeatureEngagementTracker(Tracker tracker) {
        assert tracker != null : "Tracker must not be null";
        mFeatureEngagementTracker = tracker;
    }

    Tracker getFeatureEngagementTracker() {
        assert mFeatureEngagementTracker != null : "Attempting to access null Tracker";
        return mFeatureEngagementTracker;
    }

    void setVisible(boolean visible) {
        TraceEvent.begin("KeyboardAccessoryView#setVisible");
        if (!visible || getVisibility() != VISIBLE) mBarItemsView.scrollToPosition(0);
        if (visible) {
            show();
            mBarItemsView.post(mBarItemsView::invalidateItemDecorations);
            // Animate the suggestions only if the bar wasn't visible already.
            if (getVisibility() != View.VISIBLE) animateSuggestionArrival();
        } else {
            hide();
        }
        TraceEvent.end("KeyboardAccessoryView#setVisible");
    }

    void setSkipClosingAnimation(boolean shouldSkipClosingAnimation) {
        mShouldSkipClosingAnimation = shouldSkipClosingAnimation;
    }

    void setAnimationListener(AnimationListener animationListener) {
        mAnimationListener = animationListener;
    }

    ViewRectProvider getSwipingIphRect() {
        View lastChild = getLastChild();
        if (lastChild == null) return null;
        ViewRectProvider provider = new ViewRectProvider(getLastChild());
        provider.setIncludePadding(true);
        return provider;
    }

    void setBottomOffset(int bottomOffset) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.setMargins(params.leftMargin, params.topMargin, params.rightMargin, bottomOffset);
        setLayoutParams(params);
    }

    void setObfuscatedLastChildAt(Callback<Integer> obfuscatedLastChildAt) {
        mObfuscatedLastChildAt = obfuscatedLastChildAt;
    }

    void setOnTouchEventCallback(Callback<Boolean> onTouchEvent) {
        mOnTouchEvent = onTouchEvent;
    }

    void disableAnimationsForTesting() {
        mDisableAnimations = true;
    }

    boolean areAnimationsDisabled() {
        return mDisableAnimations;
    }

    void setAllowClicksWhileObscured(boolean allowClicksWhileObscured) {
        mAllowClicksWhileObscured = allowClicksWhileObscured;
    }

    boolean areClicksAllowedWhenObscured() {
        return mAllowClicksWhileObscured;
    }

    void setAccessibilityMessage(boolean hasSuggestions) {
        setContentDescription(
                getContext()
                        .getString(
                                hasSuggestions
                                        ? R.string.autofill_keyboard_accessory_content_description
                                        : R.string
                                                .autofill_keyboard_accessory_content_fallback_description));
    }

    void setBarItemsAdapter(RecyclerView.Adapter adapter) {
        // Make sure the view updates the fallback icon padding whenever new items arrive.
        adapter.registerAdapterDataObserver(
                new RecyclerView.AdapterDataObserver() {
                    @Override
                    public void onItemRangeChanged(int positionStart, int itemCount) {
                        super.onItemRangeChanged(positionStart, itemCount);
                        mBarItemsView.scrollToPosition(0);
                        mBarItemsView.invalidateItemDecorations();
                        onItemsChanged();
                    }
                });
        mBarItemsView.setAdapter(adapter);
    }

    private void show() {
        TraceEvent.begin("KeyboardAccessoryView#show");
        bringToFront(); // Needs to overlay every component and the bottom sheet - like a keyboard.
        if (mRunningAnimation != null) {
            mRunningAnimation.cancel();
            mRunningAnimation = null;
        }
        if (areAnimationsDisabled()) {
            mRunningAnimation = null;
            setVisibility(View.VISIBLE);
            return;
        }
        if (getVisibility() != View.VISIBLE) setAlpha(0f);
        mRunningAnimation =
                animate()
                        .alpha(1f)
                        .setDuration(FADE_ANIMATION_DURATION_MS)
                        .setInterpolator(new AccelerateInterpolator())
                        .withStartAction(() -> setVisibility(View.VISIBLE))
                        .withEndAction(
                                () -> {
                                    mAnimationListener.onFadeInEnd();
                                    mRunningAnimation = null;
                                });
        announceForAccessibility(getContentDescription());
        TraceEvent.end("KeyboardAccessoryView#show");
    }

    private void hide() {
        if (mRunningAnimation != null) {
            mRunningAnimation.cancel();
            mRunningAnimation = null;
        }
        if (mShouldSkipClosingAnimation || areAnimationsDisabled()) {
            mRunningAnimation = null;
            setVisibility(View.GONE);
            return;
        }
        mRunningAnimation =
                animate()
                        .alpha(0.0f)
                        .setInterpolator(new AccelerateInterpolator())
                        .setStartDelay(HIDING_ANIMATION_DELAY_MS)
                        .setDuration(FADE_ANIMATION_DURATION_MS - HIDING_ANIMATION_DELAY_MS)
                        .withEndAction(
                                () -> {
                                    setVisibility(View.GONE);
                                    mRunningAnimation = null;
                                });
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

    private void onItemsChanged() {
        if (isLastChildObfuscated()) {
            mObfuscatedLastChildAt.onResult(mBarItemsView.indexOfChild(getLastChild()));
        }
    }

    private View getLastChild() {
        for (int i = mBarItemsView.getChildCount() - 1; i >= 0; --i) {
            View lastChild = mBarItemsView.getChildAt(i);
            if (lastChild == null) continue;
            return lastChild;
        }
        return null;
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

    private void initializeHorizontalRecyclerView(RecyclerView recyclerView) {
        // Set horizontal layout.
        recyclerView.setLayoutManager(
                new LinearLayoutManager(getContext(), LinearLayoutManager.HORIZONTAL, false));

        int pad =
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_horizontal_padding);

        // Remove all animations - the accessory shouldn't be visibly built anyway.
        recyclerView.setItemAnimator(null);

        recyclerView.setPaddingRelative(pad, 0, 0, 0);
    }

    @VisibleForTesting
    boolean hasRunningAnimation() {
        return mRunningAnimation != null;
    }
}
