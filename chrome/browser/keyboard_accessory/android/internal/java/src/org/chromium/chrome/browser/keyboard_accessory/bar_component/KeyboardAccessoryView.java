// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.LinearLayout;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryStyle.NotchPosition;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
@NullMarked
class KeyboardAccessoryView extends LinearLayout {
    private static final int ARRIVAL_ANIMATION_DURATION_MS = 300;
    private static final float ARRIVAL_ANIMATION_BOUNCE_LENGTH_DIP = 200f;
    private static final float ARRIVAL_ANIMATION_TENSION = 1f;
    private static final int FADE_ANIMATION_DURATION_MS = 150; // Total duration of show/hide.
    private static final int HIDING_ANIMATION_DELAY_MS = 50; // Shortens animation duration.

    private @Nullable Tracker mFeatureEngagementTracker;
    private @Nullable Callback<Integer> mObfuscatedLastChildAt;
    private @Nullable Callback<Boolean> mOnTouchEvent;
    private @Nullable ObjectAnimator mAnimator;
    private @Nullable AnimationListener mAnimationListener;
    private @Nullable ViewPropertyAnimator mRunningAnimation;
    private boolean mShouldSkipClosingAnimation;
    private boolean mDisableAnimations;
    private boolean mDisableAnimationsForced;
    private boolean mAllowClicksWhileObscured;
    private boolean mHasStickyLastItem;
    private int mMaxWidth;
    private int mHorizontalOffset;
    private boolean mAnimateSuggestionsFromTop;
    private boolean mIsUndocked;

    protected RecyclerView mBarItemsView;
    protected RecyclerView mFixedBarItemsView;

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
                public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                    if (newState != RecyclerView.SCROLL_STATE_IDLE) {
                        mBarItemsView.removeOnScrollListener(mScrollingIphCallback);
                        if (mFeatureEngagementTracker != null) {
                            KeyboardAccessoryIphUtils.emitScrollingEvent(mFeatureEngagementTracker);
                        }
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
            if (parent.getAdapter() == null
                    || !isLastItem(parent, view, state.getItemCount())
                    || !mHasStickyLastItem) {
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
                            .getDimensionPixelSize(R.dimen.keyboard_accessory_tab_icon_width);
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

    /** Constructor for inflating from XML which is why it must be public. */
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
        if (mOnTouchEvent != null) {
            mOnTouchEvent.onResult(shouldFilterEvent);
        }

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

        // TODO: crbug.com/385172647 - Move height parameters to the xml file once the feature is
        // launched.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
            LinearLayout barContents = findViewById(R.id.accessory_bar_contents);
            barContents.setMinimumHeight(
                    getResources()
                            .getDimensionPixelSize(R.dimen.keyboard_accessory_height_redesign));

            LinearLayout.LayoutParams layoutParams =
                    (LinearLayout.LayoutParams) barContents.getLayoutParams();
            layoutParams.height =
                    getResources()
                            .getDimensionPixelSize(R.dimen.keyboard_accessory_height_redesign);
            barContents.setLayoutParams(layoutParams);
        }

        mBarItemsView = findViewById(R.id.bar_items_view);
        initializeHorizontalRecyclerView(mBarItemsView);
        mFixedBarItemsView = findViewById(R.id.fixed_bar_items_view);
        mFixedBarItemsView.setLayoutManager(
                new LinearLayoutManager(getContext(), LinearLayoutManager.HORIZONTAL, false));

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

    @EnsuresNonNull("mFeatureEngagementTracker")
    void setFeatureEngagementTracker(Tracker tracker) {
        mFeatureEngagementTracker = assumeNonNull(tracker);
    }

    @Nullable
    Tracker getFeatureEngagementTracker() {
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

    @Nullable
    ViewRectProvider getSwipingIphRect() {
        @Nullable View lastChild = getLastChild();
        if (lastChild == null) return null;
        ViewRectProvider provider = new ViewRectProvider(lastChild);
        provider.setIncludePadding(true);
        return provider;
    }

    void setStyle(KeyboardAccessoryStyle style) {
        mMaxWidth = style.getMaxWidth();
        mHorizontalOffset = style.getHorizontalOffset();
        mIsUndocked = !style.isDocked();
        CoordinatorLayout.LayoutParams params = (CoordinatorLayout.LayoutParams) getLayoutParams();
        if (style.isDocked()) {
            applyDockedStyle(params, style);
        } else {
            applyUndockedStyle(params, style);
        }
        setLayoutParams(params);
    }

    /**
     * Configures the view's appearance for the floating (undocked) state. This state has an
     * elevation, rounded corners and wraps its content.
     */
    @SuppressLint("RtlHardcoded")
    private void applyUndockedStyle(
            CoordinatorLayout.LayoutParams params, KeyboardAccessoryStyle style) {
        boolean isDynamicPositioningEnabled =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING);
        // To provide an experience similar to desktop popup, animations are disabled.
        mDisableAnimations = isDynamicPositioningEnabled;
        if (isDynamicPositioningEnabled) {
            // For dynamically positioned keyboard accessory, the keyboard accessory is positioned
            // by setting the gravity to LEFT|TOP. The horizontal positioning is handled in
            // onMeasure/onLayout to allow for width adjustment, while setMargins applies only
            // the vertical offset. Gravity.LEFT is used even for RTL layout.
            params.gravity = Gravity.LEFT | Gravity.TOP;
            params.setMargins(0, style.getVerticalOffset(), 0, 0);
        } else {
            // For statically positioned keyboard accessory, the gravity is centered horizontally
            // and at the top of the parent. Only a vertical offset is used.
            params.gravity = Gravity.CENTER | Gravity.TOP;
            params.setMargins(0, style.getVerticalOffset(), 0, 0);
        }
        params.width = ViewGroup.LayoutParams.WRAP_CONTENT;

        // accesory_shadow is not used for an undocked rounded bar, which uses elevation instead.
        findViewById(R.id.accessory_shadow).setVisibility(View.GONE);
        findViewById(R.id.accessory_bar_contents).setBackground(null);

        if (isDynamicPositioningEnabled) {
            // For the dynamic positioning the notch is displayed by outlining a background.
            // This code path can be used only when AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN
            // flag is enabled.
            setBackgroundResource(R.color.default_bg_color_baseline);
            @Px
            int notchHeight =
                    getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_notch_height);
            @NotchPosition int notchPosition = style.getNotchPosition();
            assert notchPosition != NotchPosition.HIDDEN;
            if (notchPosition == NotchPosition.TOP) {
                setPadding(getPaddingStart(), notchHeight, getPaddingEnd(), 0);
            } else if (notchPosition == NotchPosition.BOTTOM) {
                setPadding(getPaddingStart(), 0, getPaddingEnd(), notchHeight);
            }
            setOutlineProvider(new NotchedKeyboardAccessoryOutlineProvider(notchPosition));
            setClipToOutline(true);
        } else {
            // For the static positioning the rounded background is implemented using a static
            // drawable.
            setBackgroundResource(R.drawable.keyboard_accessory_shadow_shape);
            if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)) {
                GradientDrawable background = (GradientDrawable) getBackground();
                background.setCornerRadius(
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.keyboard_accessory_corner_radius_redesign));
            }
        }
        @Px
        int elevation = getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_elevation);
        setElevation(elevation);
    }

    /**
     * Configures the view's appearance for the standard (docked) bottom state. This state matches
     * the parent width and has no elevation.
     */
    private void applyDockedStyle(
            CoordinatorLayout.LayoutParams params, KeyboardAccessoryStyle style) {
        mDisableAnimations = false;
        params.gravity = Gravity.BOTTOM;
        params.setMargins(0, 0, 0, style.getVerticalOffset());
        params.width = ViewGroup.LayoutParams.MATCH_PARENT;
        setTranslationX(0);

        findViewById(R.id.accessory_shadow).setVisibility(View.VISIBLE);
        findViewById(R.id.accessory_bar_contents)
                .setBackgroundResource(R.color.default_bg_color_baseline);
        setBackground(null);
        setElevation(0);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int availableWidth = MeasureSpec.getSize(widthMeasureSpec);
        widthMeasureSpec = calculateAccessoryWidthMeasureSpec(widthMeasureSpec);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (mIsUndocked
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList
                                .AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)) {
            adjustPositionAndNotch(availableWidth);
        }
    }

    private int calculateAccessoryWidthMeasureSpec(int widthMeasureSpec) {
        // If a maximum width is defined, ensure the MeasureSpec does not exceed it.
        int availableWidth = MeasureSpec.getSize(widthMeasureSpec);
        int maxWidth = availableWidth;
        if (mMaxWidth > 0) {
            maxWidth = Math.min(mMaxWidth, maxWidth);
        }

        if (mIsUndocked
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList
                                .AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)) {
            int horizontalMargin =
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen
                                            .keyboard_accessory_bar_dynamic_positioning_horizontal_margin);
            // Reduce the max width by the margins on both sides.
            maxWidth = Math.min(maxWidth, availableWidth - 2 * horizontalMargin);
        }

        // If the calculated max width is less than the available width, update the MeasureSpec
        // to enforce the new maximum width.
        if (maxWidth < availableWidth) {
            int measureMode = MeasureSpec.getMode(widthMeasureSpec);
            widthMeasureSpec = MeasureSpec.makeMeasureSpec(maxWidth, measureMode);
        }
        return widthMeasureSpec;
    }

    private void adjustPositionAndNotch(int availableWidth) {
        // This shifts the keyboard accessory horizontally to the left when can't grow to the right
        // because of the viewport border.
        int translationX = mHorizontalOffset;
        int horizontalMargin =
                getResources()
                        .getDimensionPixelSize(
                                R.dimen
                                        .keyboard_accessory_bar_dynamic_positioning_horizontal_margin);
        // If the preferred offset plus the view's width exceeds the viewport width, the view is
        // bleeding off the right edge.
        int maxTranslationX = availableWidth - getMeasuredWidth() - horizontalMargin;

        // Clamp to the right edge (prevent bleeding off the right side).
        translationX = Math.min(translationX, maxTranslationX);

        // Clamp to the left edge (prevent bleeding off the left side).
        translationX = Math.max(translationX, horizontalMargin);

        setTranslationX(translationX);

        // The notch needs to be moved to point to the focused field when the accessory is
        // shifted.
        if (getOutlineProvider() instanceof NotchedKeyboardAccessoryOutlineProvider) {
            NotchedKeyboardAccessoryOutlineProvider provider =
                    (NotchedKeyboardAccessoryOutlineProvider) getOutlineProvider();

            // Keyboard Accessory shifted left (translationX < mHorizontalOffset)
            // The accessory is pushed left to fit on screen. The notch must move right
            // relative to the view to stay aligned with the field.
            //
            // When Keyboard Accessory shifted right (translationX > mHorizontalOffset)
            // The accessory is pushed right to fit on screen (e.g. clamped by left margin).
            // The notch doesn't need to be moved relatively to the Keyboard Accessory, because its
            // defaupt position is the correct one.
            int notchOffsetX = Math.max(0, mHorizontalOffset - translationX);

            provider.setNotchOffsetX(notchOffsetX);
            // Invalidating triggers regenerating the notch in the correct place.
            invalidateOutline();
        }
    }

    void setObfuscatedLastChildAt(Callback<Integer> obfuscatedLastChildAt) {
        mObfuscatedLastChildAt = obfuscatedLastChildAt;
    }

    void setOnTouchEventCallback(Callback<Boolean> onTouchEvent) {
        mOnTouchEvent = onTouchEvent;
    }

    void disableAnimationsForTesting() {
        mDisableAnimationsForced = true;
    }

    /**
     * Returns true if animations are disabled by the runtime logic or if the test forced animations
     * to be disabled.
     */
    boolean areAnimationsDisabled() {
        return mDisableAnimationsForced || mDisableAnimations;
    }

    void setAllowClicksWhileObscured(boolean allowClicksWhileObscured) {
        mAllowClicksWhileObscured = allowClicksWhileObscured;
    }

    boolean areClicksAllowedWhenObscured() {
        return mAllowClicksWhileObscured;
    }

    void setHasStickyLastItem(boolean hasStickyLastItem) {
        mHasStickyLastItem = hasStickyLastItem;
    }

    void setAnimateSuggestionsFromTop(boolean animateSuggestionsFromTop) {
        mAnimateSuggestionsFromTop = animateSuggestionsFromTop;
    }

    void setAccessibilityMessage(boolean hasSuggestions) {
        int descriptionId =
                hasSuggestions
                        ? R.string.autofill_keyboard_accessory_content_description
                        : R.string.autofill_keyboard_accessory_content_fallback_description;
        setContentDescription(getContext().getString(descriptionId));
    }

    void setBarItemsAdapter(RecyclerView.Adapter adapter) {
        registerAdapter(adapter, mBarItemsView);
    }

    void setFixedBarItemsAdapter(RecyclerView.Adapter adapter) {
        registerAdapter(adapter, mFixedBarItemsView);
    }

    private void registerAdapter(RecyclerView.Adapter adapter, RecyclerView view) {
        // Make sure the view updates the fallback icon padding whenever new items arrive.
        adapter.registerAdapterDataObserver(
                new RecyclerView.AdapterDataObserver() {
                    @Override
                    public void onItemRangeChanged(int positionStart, int itemCount) {
                        super.onItemRangeChanged(positionStart, itemCount);
                        view.scrollToPosition(0);
                        view.invalidateItemDecorations();
                        onItemsChanged();
                    }
                });
        view.setAdapter(adapter);
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
                                    if (mAnimationListener != null) {
                                        mAnimationListener.onFadeInEnd();
                                    }
                                    mRunningAnimation = null;
                                });
        ViewCompat.setAccessibilityPaneTitle(this, getContentDescription());
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
        if (mObfuscatedLastChildAt != null && isLastChildObfuscated()) {
            mObfuscatedLastChildAt.onResult(mBarItemsView.indexOfChild(getLastChild()));
        }
    }

    private @Nullable View getLastChild() {
        for (int i = mBarItemsView.getChildCount() - 1; i >= 0; --i) {
            View lastChild = mBarItemsView.getChildAt(i);
            if (lastChild == null) continue;
            return lastChild;
        }
        return null;
    }

    private void animateSuggestionArrival() {
        if (areAnimationsDisabled()) return;
        if (mAnimator != null && mAnimator.isRunning()) {
            mAnimator.cancel();
        }

        if (mAnimateSuggestionsFromTop) {
            mAnimator = createVerticalAnimator();
        } else {
            mAnimator = createHorizontalAnimator();
        }
        mAnimator.setDuration(ARRIVAL_ANIMATION_DURATION_MS);
        mAnimator.setInterpolator(new OvershootInterpolator(ARRIVAL_ANIMATION_TENSION));
        mAnimator.start();
    }

    private ObjectAnimator createVerticalAnimator() {
        // Animate from the top of the screen to its original position.
        final float start = -getY();
        setTranslationY(start);
        return ObjectAnimator.ofFloat(this, "translationY", start, 0f);
    }

    private ObjectAnimator createHorizontalAnimator() {
        final float endPosition = 0f;
        int bounceDirection = getLayoutDirection() == LAYOUT_DIRECTION_RTL ? 1 : -1;
        float start =
                endPosition
                        - bounceDirection
                                * ARRIVAL_ANIMATION_BOUNCE_LENGTH_DIP
                                * getContext().getResources().getDisplayMetrics().density;
        mBarItemsView.setTranslationX(start);
        return ObjectAnimator.ofFloat(mBarItemsView, "translationX", start, endPosition);
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
