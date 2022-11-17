// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.view.accessibility.AccessibilityEvent;
import android.view.animation.AccelerateInterpolator;
import android.widget.LinearLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
class KeyboardAccessoryView extends LinearLayout {
    protected static final int FADE_ANIMATION_DURATION_MS = 150; // Total duration of show/hide.
    protected static final int HIDING_ANIMATION_DELAY_MS = 50; // Shortens animation duration.

    protected RecyclerView mBarItemsView;
    protected TabLayout mTabLayout;
    private ViewPropertyAnimator mRunningAnimation;
    private boolean mShouldSkipClosingAnimation;
    private boolean mDisableAnimations;

    /** Interface that allows to react to animations. */
    interface AnimationListener {
        /**
         * Called if the accessory bar stopped fading in. The fade-in only happens sometimes, e.g.
         * if the bar is already visible or animations are disabled, this signal is not issued.
         */
        void onFadeInEnd();
    }
    private AnimationListener mAnimationListener;

    protected static class HorizontalDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final int mHorizontalMargin;

        HorizontalDividerItemDecoration(int horizontalMargin) {
            this.mHorizontalMargin = horizontalMargin;
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

        protected int getItemOffsetInternal(
                final View view, final RecyclerView parent, RecyclerView.State state) {
            return mHorizontalMargin;
        }
    }

    /**
     * Constructor for inflating from XML.
     */
    public KeyboardAccessoryView(Context context, AttributeSet attrs) {
        super(context, attrs);
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
        setOnTouchListener((view, motionEvent) -> {
            performClick(); // Setting a touch listener requires this call which is a NoOp.
            return true; // Return that the motionEvent was consumed and needs no further handling.
        });
        setOnClickListener(view -> {});
        setClickable(false); // Disables the "Double-tap to activate" Talkback reading.
        setSoundEffectsEnabled(false);
        TraceEvent.end("KeyboardAccessoryView#onFinishInflate");
    }

    TabLayout getTabLayout() {
        if (mTabLayout == null) {
            mTabLayout = findViewById(R.id.tabs);
        }
        return mTabLayout;
    }

    void setVisible(boolean visible) {
        TraceEvent.begin("KeyboardAccessoryView#setVisible");
        if (!visible || getVisibility() != VISIBLE) mBarItemsView.scrollToPosition(0);
        if (visible) {
            show();
        } else {
            hide();
        }
        TraceEvent.end("KeyboardAccessoryView#setVisible");
    }

    void setBottomOffset(int bottomOffset) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.setMargins(params.leftMargin, params.topMargin, params.rightMargin, bottomOffset);
        setLayoutParams(params);
    }

    void setBarItemsAdapter(RecyclerView.Adapter adapter) {
        // Make sure the view updates the fallback icon padding whenever new items arrive.
        adapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
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

    void setAnimationListener(AnimationListener animationListener) {
        mAnimationListener = animationListener;
    }

    /** Template method. Override to be notified if the bar items change. */
    @CallSuper
    protected void onItemsChanged() {}

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
        mRunningAnimation = animate()
                                    .alpha(1f)
                                    .setDuration(FADE_ANIMATION_DURATION_MS)
                                    .setInterpolator(new AccelerateInterpolator())
                                    .withStartAction(() -> setVisibility(View.VISIBLE))
                                    .withEndAction(() -> {
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
                        .withEndAction(() -> {
                            setVisibility(View.GONE);
                            mRunningAnimation = null;
                        });
    }

    void setSkipClosingAnimation(boolean shouldSkipClosingAnimation) {
        mShouldSkipClosingAnimation = shouldSkipClosingAnimation;
    }

    void disableAnimationsForTesting() {
        mDisableAnimations = true;
    }

    boolean areAnimationsDisabled() {
        return mDisableAnimations;
    }

    private void initializeHorizontalRecyclerView(RecyclerView recyclerView) {
        // Set horizontal layout.
        recyclerView.setLayoutManager(
                new LinearLayoutManager(getContext(), LinearLayoutManager.HORIZONTAL, false));

        int pad =
                getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_horizontal_padding);
        // Create margins between every element.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            recyclerView.addItemDecoration(new HorizontalDividerItemDecoration(pad));
        }

        // Remove all animations - the accessory shouldn't be visibly built anyway.
        recyclerView.setItemAnimator(null);

        ViewCompat.setPaddingRelative(recyclerView, pad, 0, 0, 0);
    }

    @VisibleForTesting
    boolean hasRunningAnimation() {
        return mRunningAnimation != null;
    }
}
