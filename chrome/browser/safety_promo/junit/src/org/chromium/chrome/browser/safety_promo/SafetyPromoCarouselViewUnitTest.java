// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Rect;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SafetyPromoCarouselView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoCarouselViewUnitTest {
    // Time between two dispatched events, like one frame at 60 Hz.
    private static final long EVENT_INTERVAL_MS = 16;
    private static final int VIEW_WIDTH_PX = 1200;
    private static final int VIEW_HEIGHT_PX = 800;
    // A page other than the initial one, so that scrolling to it is observable.
    private static final int TEST_PAGE_INDEX = 1;

    private Context mContext;
    private int mTouchSlop;
    private SafetyPromoCarouselView mView;

    private long mDownTimeMs;
    private long mEventTimeMs;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTouchSlop = ViewConfiguration.get(mContext).getScaledTouchSlop();
        mView =
                inflateView(
                        R.layout.safety_promo_fre_carousel_portrait_view,
                        R.layout.safety_promo_carousel_portrait_illustration);
    }

    @Test
    public void testOnFinishInflate() {
        assertNotNull(mView.getRecyclerView());
        assertNotNull(mView.findViewById(R.id.safety_promo_carousel_title));
        assertNotNull(mView.findViewById(R.id.safety_promo_carousel_subtitle));
        assertNotNull(mView.findViewById(R.id.fre_continue_button));
        assertNotNull(mView.findViewById(R.id.safety_promo_carousel_page_indicator));
    }

    @Test
    public void testSetTitleText() {
        int titleResId = R.string.safety_fre_promo_password_manager_carousel_title;
        mView.setTitleText(titleResId);

        TextView titleView = mView.findViewById(R.id.safety_promo_carousel_title);
        assertEquals(mContext.getString(titleResId), titleView.getText().toString());
    }

    @Test
    public void testSetSubtitleText() {
        int subtitleResId = R.string.safety_fre_promo_password_manager_carousel_subtitle;
        mView.setSubtitleText(subtitleResId);

        TextView subtitleView = mView.findViewById(R.id.safety_promo_carousel_subtitle);
        assertEquals(mContext.getString(subtitleResId), subtitleView.getText().toString());
    }

    @Test
    public void testSetContinueButtonOnClickListener() {
        View.OnClickListener mockListener = mock(View.OnClickListener.class);
        mView.setContinueButtonOnClickListener(mockListener);

        ButtonCompat continueButton = mView.findViewById(R.id.fre_continue_button);
        continueButton.performClick();

        verify(mockListener).onClick(continueButton);
    }

    @Test
    public void testSetPageIndicatorCount() {
        mView.setPageIndicatorCount(5);

        SafetyPromoPageIndicatorView indicatorView =
                mView.findViewById(R.id.safety_promo_carousel_page_indicator);
        assertEquals(5, indicatorView.getPageCountForTesting());
    }

    @Test
    public void testSetActivePage_movesIndicatorAndScrolls() {
        mView.setActivePage(TEST_PAGE_INDEX);

        SafetyPromoPageIndicatorView indicatorView =
                mView.findViewById(R.id.safety_promo_carousel_page_indicator);
        assertEquals(TEST_PAGE_INDEX, indicatorView.getActivePositionForTesting());
        // The scroll is applied on the next layout.
        layoutView(mView);
        assertEquals(TEST_PAGE_INDEX, getVisiblePage(mView));
    }

    @Test
    public void testHorizontalSwipeFromTitle_portrait_isForwarded() {
        assertHorizontalSwipeIsForwarded(mView, getTitleCenter(mView));
    }

    @Test
    public void testHorizontalSwipeFromTitle_landscape_isForwarded() {
        SafetyPromoCarouselView view = inflateLandscapeView();
        assertHorizontalSwipeIsForwarded(view, getTitleCenter(view));
    }

    @Test
    public void testHorizontalSwipeFromRightPanePadding_landscape_isForwarded() {
        // No child consumes ACTION_DOWN on the padding, so the view keeps the gesture itself.
        SafetyPromoCarouselView view = inflateLandscapeView();
        View buttons = view.findViewById(R.id.safety_promo_button_container);
        float[] buttonsCenter = getCenterInView(buttons, view);
        float[] start = {buttonsCenter[0] - buttons.getWidth() / 2f - 1, buttonsCenter[1]};
        assertHorizontalSwipeIsForwarded(view, start);
    }

    @Test
    public void testTwoPointerHorizontalSwipeFromTitle_portrait_isForwarded() {
        RecyclerView recyclerView = mView.getRecyclerView();
        float[] first = getTitleCenter(mView);
        float[] second = {first[0] + 2 * mTouchSlop, first[1]};

        dispatchEvent(mView, MotionEvent.ACTION_DOWN, first, 0);
        dispatchTwoPointerEvent(mView, MotionEvent.ACTION_POINTER_DOWN, first, second, 0);
        dispatchTwoPointerEvent(mView, MotionEvent.ACTION_MOVE, first, second, -3 * mTouchSlop);
        assertEquals(RecyclerView.SCROLL_STATE_DRAGGING, recyclerView.getScrollState());

        dispatchTwoPointerEvent(mView, MotionEvent.ACTION_MOVE, first, second, -6 * mTouchSlop);
        assertTrue(recyclerView.computeHorizontalScrollOffset() > 0);
    }

    @Test
    public void testHorizontalSwipeWithinTouchSlopFromTitle_portrait_isNotForwarded() {
        List<Integer> recyclerViewActions = recordTouchActions(mView.getRecyclerView());
        float[] start = getTitleCenter(mView);

        dispatchEvent(mView, MotionEvent.ACTION_DOWN, start, 0);
        dispatchEvent(mView, MotionEvent.ACTION_MOVE, start, -mTouchSlop);
        dispatchEvent(mView, MotionEvent.ACTION_UP, start, -mTouchSlop);

        assertTrue(recyclerViewActions.isEmpty());
    }

    @Test
    public void testHorizontalSwipeInsideRecyclerView_landscape_reachesRecyclerViewUnchanged() {
        SafetyPromoCarouselView view = inflateLandscapeView();
        List<Integer> recyclerViewActions = recordTouchActions(view.getRecyclerView());
        float[] start = getCenterInView(view.getRecyclerView(), view);

        dispatchEvent(view, MotionEvent.ACTION_DOWN, start, 0);
        dispatchEvent(view, MotionEvent.ACTION_MOVE, start, -3 * mTouchSlop);
        dispatchEvent(view, MotionEvent.ACTION_MOVE, start, -6 * mTouchSlop);

        // Verify no forwarding via the event sequence, since forwarding would scroll too, but via
        // ACTION_CANCEL and a synthesized ACTION_DOWN: [DOWN, CANCEL, DOWN, MOVE, MOVE].
        assertEquals(
                List.of(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_MOVE),
                recyclerViewActions);
    }

    @Test
    public void testMostlyVerticalSwipeFromTitle_portrait_scrollsScrollView() {
        List<Integer> recyclerViewActions = recordTouchActions(mView.getRecyclerView());
        ScrollView scrollView = mView.findViewById(R.id.safety_promo_carousel_scroll_view);
        float[] start = getTitleCenter(mView);

        // Beyond the touch slop horizontally, but mostly vertical.
        dispatchEvent(mView, MotionEvent.ACTION_DOWN, start, 0, 0);
        dispatchEvent(mView, MotionEvent.ACTION_MOVE, start, -2 * mTouchSlop, -3 * mTouchSlop);
        dispatchEvent(mView, MotionEvent.ACTION_MOVE, start, -4 * mTouchSlop, -6 * mTouchSlop);

        assertTrue(scrollView.getScrollY() > 0);
        assertTrue(recyclerViewActions.isEmpty());
    }

    /**
     * Swipes left from {@code start}, and checks that the RecyclerView starts dragging on the first
     * ACTION_MOVE, scrolls forward, and stops dragging on ACTION_UP.
     */
    private void assertHorizontalSwipeIsForwarded(SafetyPromoCarouselView view, float[] start) {
        RecyclerView recyclerView = view.getRecyclerView();

        dispatchEvent(view, MotionEvent.ACTION_DOWN, start, 0);
        dispatchEvent(view, MotionEvent.ACTION_MOVE, start, -3 * mTouchSlop);
        assertEquals(RecyclerView.SCROLL_STATE_DRAGGING, recyclerView.getScrollState());

        dispatchEvent(view, MotionEvent.ACTION_MOVE, start, -6 * mTouchSlop);
        assertTrue(recyclerView.computeHorizontalScrollOffset() > 0);

        dispatchEvent(view, MotionEvent.ACTION_UP, start, -6 * mTouchSlop);
        assertNotEquals(RecyclerView.SCROLL_STATE_DRAGGING, recyclerView.getScrollState());
    }

    private void dispatchEvent(View view, int action, float[] start, float dx) {
        dispatchEvent(view, action, start, dx, /* dy= */ 0);
    }

    private void dispatchEvent(View view, int action, float[] start, float dx, float dy) {
        mEventTimeMs += EVENT_INTERVAL_MS;
        if (action == MotionEvent.ACTION_DOWN) mDownTimeMs = mEventTimeMs;
        MotionEvent event =
                MotionEvent.obtain(
                        mDownTimeMs,
                        mEventTimeMs,
                        action,
                        start[0] + dx,
                        start[1] + dy,
                        /* metaState= */ 0);
        view.dispatchTouchEvent(event);
        event.recycle();
    }

    /**
     * Dispatches an event with pointers 0 and 1 at {@code first} and {@code second}, both moved
     * horizontally by {@code dx}. For ACTION_POINTER_DOWN, the second pointer is the one going
     * down.
     */
    private void dispatchTwoPointerEvent(
            View view, int action, float[] first, float[] second, float dx) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[2];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[2];
        float[][] points = {first, second};
        for (int i = 0; i < 2; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
            coords[i].x = points[i][0] + dx;
            coords[i].y = points[i][1];
        }
        if (action == MotionEvent.ACTION_POINTER_DOWN) {
            action |= 1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT;
        }
        mEventTimeMs += EVENT_INTERVAL_MS;
        MotionEvent event =
                MotionEvent.obtain(
                        mDownTimeMs,
                        mEventTimeMs,
                        action,
                        /* pointerCount= */ 2,
                        properties,
                        coords,
                        /* metaState= */ 0,
                        /* buttonState= */ 0,
                        /* xPrecision= */ 1f,
                        /* yPrecision= */ 1f,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        InputDevice.SOURCE_TOUCHSCREEN,
                        /* flags= */ 0);
        view.dispatchTouchEvent(event);
        event.recycle();
    }

    /**
     * Records the masked action of every touch event that {@code view} receives into the returned
     * list, without consuming the events.
     */
    private static List<Integer> recordTouchActions(View view) {
        List<Integer> actions = new ArrayList<>();
        view.setOnTouchListener(
                (v, event) -> {
                    actions.add(event.getActionMasked());
                    return false;
                });
        return actions;
    }

    private SafetyPromoCarouselView inflateLandscapeView() {
        return inflateView(
                R.layout.safety_promo_fre_carousel_landscape_view,
                R.layout.safety_promo_carousel_landscape_illustration);
    }

    /** Inflates and lays out {@code layoutId}, and gives its RecyclerView items. */
    private SafetyPromoCarouselView inflateView(int layoutId, int itemLayoutId) {
        SafetyPromoCarouselView view =
                (SafetyPromoCarouselView)
                        LayoutInflater.from(mContext).inflate(layoutId, /* root= */ null);
        RecyclerView recyclerView = view.getRecyclerView();
        recyclerView.setLayoutManager(
                new LinearLayoutManager(
                        mContext, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false));
        recyclerView.setAdapter(
                new SafetyPromoCarouselAdapter(
                        List.of(SafetyPromoItem.PASSWORD_MANAGER, SafetyPromoItem.INCOGNITO),
                        itemLayoutId));
        layoutView(view);
        return view;
    }

    private static void layoutView(SafetyPromoCarouselView view) {
        view.measure(
                View.MeasureSpec.makeMeasureSpec(VIEW_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(VIEW_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, VIEW_WIDTH_PX, VIEW_HEIGHT_PX);
    }

    /** Returns the position of the carousel page {@code view} shows. */
    private static int getVisiblePage(SafetyPromoCarouselView view) {
        LinearLayoutManager layoutManager =
                (LinearLayoutManager) view.getRecyclerView().getLayoutManager();
        return layoutManager.findFirstVisibleItemPosition();
    }

    private static float[] getTitleCenter(SafetyPromoCarouselView view) {
        return getCenterInView(view.findViewById(R.id.safety_promo_carousel_title), view);
    }

    /** Returns the center of {@code target} in the coordinates of its ancestor {@code root}. */
    private static float[] getCenterInView(View target, ViewGroup root) {
        Rect bounds = new Rect(0, 0, target.getWidth(), target.getHeight());
        root.offsetDescendantRectToMyCoords(target, bounds);
        return new float[] {bounds.exactCenterX(), bounds.exactCenterY()};
    }
}
