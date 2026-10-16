// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View.OnClickListener;
import android.view.ViewConfiguration;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.widget.ButtonCompat;

/**
 * View for the Safety Promo Carousel during the First Run Experience (FRE).
 *
 * <p>The RecyclerView only covers part of the view, so horizontal swipes that start elsewhere (e.g.
 * on the title, the buttons or the margins) are forwarded to it.
 */
@NullMarked
public class SafetyPromoCarouselView extends RelativeLayout {
    private final int mTouchSlop;
    private final Rect mRecyclerViewBounds = new Rect();

    private RecyclerView mRecyclerView;
    private TextView mTitleView;
    private TextView mSubtitleView;
    private ButtonCompat mContinueButton;
    private SafetyPromoPageIndicatorView mPageIndicatorView;

    private float mInitialX;
    private float mInitialY;
    private int mActivePointerId = MotionEvent.INVALID_POINTER_ID;
    private boolean mIsForwarding;

    public SafetyPromoCarouselView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mRecyclerView = findViewById(R.id.safety_promo_carousel_recycler_view);
        mTitleView = findViewById(R.id.safety_promo_carousel_title);
        mSubtitleView = findViewById(R.id.safety_promo_carousel_subtitle);
        mContinueButton = findViewById(R.id.fre_continue_button);
        mPageIndicatorView = findViewById(R.id.safety_promo_carousel_page_indicator);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            mInitialX = ev.getX();
            mInitialY = ev.getY();
            mActivePointerId = ev.getPointerId(0);
            mIsForwarding = false;
            mRecyclerViewBounds.set(0, 0, mRecyclerView.getWidth(), mRecyclerView.getHeight());
            offsetDescendantRectToMyCoords(mRecyclerView, mRecyclerViewBounds);
            return super.onInterceptTouchEvent(ev);
        }

        if (action == MotionEvent.ACTION_MOVE && !mIsForwarding && shouldStartForwarding(ev)) {
            startForwarding(ev);
            // The intercepted event never reaches onTouchEvent, so dispatch it here.
            dispatchToRecyclerView(ev);
            return true;
        }
        return super.onInterceptTouchEvent(ev);
    }

    // Clicks are handled by the children; this view only forwards swipes.
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        int action = ev.getActionMasked();
        if (!mIsForwarding) {
            // No child consumed ACTION_DOWN, e.g. on a margin. Keep the gesture so that a
            // horizontal swipe from there is forwarded too.
            if (action != MotionEvent.ACTION_MOVE || !shouldStartForwarding(ev)) return true;
            startForwarding(ev);
        }

        dispatchToRecyclerView(ev);

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            mIsForwarding = false;
        }
        return true;
    }

    /**
     * Returns whether {@code ev} is a horizontal swipe beyond the touch slop that started outside
     * the RecyclerView. Only the pointer that went down first is tracked; other pointers are
     * ignored.
     */
    private boolean shouldStartForwarding(MotionEvent ev) {
        if (mRecyclerViewBounds.contains((int) mInitialX, (int) mInitialY)) return false;
        int pointerIndex = ev.findPointerIndex(mActivePointerId);
        if (pointerIndex < 0) return false;

        float dx = Math.abs(ev.getX(pointerIndex) - mInitialX);
        float dy = Math.abs(ev.getY(pointerIndex) - mInitialY);
        return dx > mTouchSlop && dx > dy;
    }

    /**
     * Starts forwarding by sending the RecyclerView a synthesized ACTION_DOWN at the initial point.
     */
    private void startForwarding(MotionEvent ev) {
        mIsForwarding = true;
        long downTime = ev.getDownTime();
        MotionEvent downEvent =
                MotionEvent.obtain(
                        downTime,
                        /* eventTime= */ downTime,
                        MotionEvent.ACTION_DOWN,
                        mInitialX,
                        mInitialY,
                        ev.getMetaState());
        dispatchToRecyclerView(downEvent);
        downEvent.recycle();
    }

    /** Dispatches a copy of {@code event} to the RecyclerView, offset into its coordinates. */
    private void dispatchToRecyclerView(MotionEvent event) {
        MotionEvent eventCopy = MotionEvent.obtain(event);
        eventCopy.offsetLocation(-mRecyclerViewBounds.left, -mRecyclerViewBounds.top);
        mRecyclerView.dispatchTouchEvent(eventCopy);
        eventCopy.recycle();
    }

    public void setContinueButtonOnClickListener(OnClickListener listener) {
        mContinueButton.setOnClickListener(listener);
    }

    public void setTitleText(@StringRes int stringResId) {
        mTitleView.setText(stringResId);
    }

    public void setSubtitleText(@StringRes int stringResId) {
        mSubtitleView.setText(stringResId);
    }

    public void setPageIndicatorCount(int count) {
        mPageIndicatorView.setPageCount(count);
    }

    /**
     * Shows the page at {@code position}: moves the page indicator and scrolls the carousel to it.
     */
    public void setActivePage(int position) {
        mPageIndicatorView.setActivePosition(position);
        mRecyclerView.scrollToPosition(position);
    }

    RecyclerView getRecyclerView() {
        return mRecyclerView;
    }
}
