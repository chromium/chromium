// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Rect;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.base.DeviceInput;

/** Unit tests for {@link DownloadTrayItemView}, on when it shows its action buttons. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadTrayItemViewTest {
    private FrameLayout mRoot;
    private DownloadTrayItemView mRow;
    private View mActions;
    private View mPrimaryButton;
    private View mSecondaryButton;
    private View mOutsideView;
    private int mSecondaryButtonClicks;

    @Before
    public void setUp() {
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        // The row sits in a window with another focusable view, so that input events and focus
        // reach it as in the tray: through its parent.
        mRoot = new FrameLayout(activity);
        LayoutInflater.from(activity).inflate(R.layout.download_tray_item_row, mRoot);
        mRow = (DownloadTrayItemView) mRoot.getChildAt(0);
        mOutsideView = new View(activity);
        mOutsideView.setFocusable(true);
        mRoot.addView(mOutsideView, new FrameLayout.LayoutParams(1, 1));
        activity.setContentView(mRoot);

        mActions = mRow.findViewById(R.id.download_tray_item_actions);
        mPrimaryButton = mRow.findViewById(R.id.download_tray_item_primary_action);
        mSecondaryButton = mRow.findViewById(R.id.download_tray_item_secondary_action);
        mSecondaryButton.setOnClickListener(v -> mSecondaryButtonClicks++);
        // As a row in progress: nothing to open, and actions.
        mRow.setClickable(false);
        mRow.setFocusable(true);
        mRow.setHasActions(true);
        ShadowLooper.idleMainLooper();
        assertTrue(mOutsideView.requestFocus());
    }

    /** Sends the row's parent a hover or touch {@code action} at ({@code x}, {@code y}). */
    private void dispatch(int source, int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(0, 0, action, x, y, /* metaState= */ 0);
        event.setSource(source);
        boolean isHover =
                action == MotionEvent.ACTION_HOVER_MOVE || action == MotionEvent.ACTION_HOVER_EXIT;
        if (isHover) {
            mRoot.dispatchGenericMotionEvent(event);
        } else {
            mRoot.dispatchTouchEvent(event);
        }
        event.recycle();
        ShadowLooper.idleMainLooper();
    }

    private void mouse(int action, float x, float y) {
        dispatch(InputDevice.SOURCE_MOUSE, action, x, y);
    }

    @Test
    public void testActions_showOnlyWhileCursorIsInsideRow() {
        View title = mRow.findViewById(R.id.download_tray_item_title);
        int fullTitleWidth = title.getWidth();
        int rowHeight = mRow.getHeight();
        float rowMiddle = rowHeight / 2f;
        float belowRow = rowHeight * 2;
        assertEquals(View.GONE, mActions.getVisibility());

        // Shown, the buttons take their room from the row's text, and leave the row's height be.
        mouse(MotionEvent.ACTION_HOVER_MOVE, 10, rowMiddle);
        assertEquals(View.VISIBLE, mActions.getVisibility());
        assertTrue(mActions.getWidth() > 0);
        assertEquals(fullTitleWidth - mActions.getWidth(), title.getWidth());
        assertEquals(rowHeight, mRow.getHeight());

        // Moving onto a button, which takes the hover from the row, leaves them showing.
        Rect button = new Rect(0, 0, mSecondaryButton.getWidth(), mSecondaryButton.getHeight());
        mRow.offsetDescendantRectToMyCoords(mSecondaryButton, button);
        float x = button.centerX();
        float y = button.centerY();
        mouse(MotionEvent.ACTION_HOVER_MOVE, x, y);
        assertTrue(mSecondaryButton.isHovered());
        assertEquals(View.VISIBLE, mActions.getVisibility());

        // Android ends the hover just before a mouse press, which hides them and gives the text
        // its room back. The press still lands on the button.
        mouse(MotionEvent.ACTION_HOVER_EXIT, x, y);
        assertEquals(View.GONE, mActions.getVisibility());
        assertEquals(fullTitleWidth, title.getWidth());
        mouse(MotionEvent.ACTION_DOWN, x, y);
        mouse(MotionEvent.ACTION_UP, x, y);
        assertEquals(1, mSecondaryButtonClicks);

        // A press dragged off the row, even from where nothing is clickable, hides them, as does
        // one taken away, e.g. to scroll the list. Neither clicks.
        mouse(MotionEvent.ACTION_DOWN, 10, rowMiddle);
        assertEquals(View.VISIBLE, mActions.getVisibility());
        mouse(MotionEvent.ACTION_MOVE, 10, belowRow);
        assertEquals(View.GONE, mActions.getVisibility());
        mouse(MotionEvent.ACTION_UP, 10, belowRow);
        mouse(MotionEvent.ACTION_DOWN, x, y);
        mouse(MotionEvent.ACTION_CANCEL, x, y);
        assertEquals(View.GONE, mActions.getVisibility());
        assertEquals(1, mSecondaryButtonClicks);

        // The hover resumes on release, and ends for good once the cursor leaves the row.
        mouse(MotionEvent.ACTION_HOVER_MOVE, x, y);
        assertEquals(View.VISIBLE, mActions.getVisibility());
        mouse(MotionEvent.ACTION_HOVER_MOVE, 10, belowRow);
        assertEquals(View.GONE, mActions.getVisibility());

        // A finger has no hover to show them with, and doesn't press what isn't there. Nor does
        // touch exploration's hover, which isn't a cursor resting on the row, show them.
        dispatch(InputDevice.SOURCE_TOUCHSCREEN, MotionEvent.ACTION_DOWN, x, y);
        dispatch(InputDevice.SOURCE_TOUCHSCREEN, MotionEvent.ACTION_UP, x, y);
        dispatch(InputDevice.SOURCE_TOUCHSCREEN, MotionEvent.ACTION_HOVER_MOVE, 10, rowMiddle);
        assertEquals(1, mSecondaryButtonClicks);
        assertEquals(View.GONE, mActions.getVisibility());

        // A row removed from under the cursor, e.g. to be reused, hides them again.
        mouse(MotionEvent.ACTION_HOVER_MOVE, 10, rowMiddle);
        assertEquals(View.VISIBLE, mActions.getVisibility());
        mRoot.removeView(mRow);
        assertEquals(View.GONE, mActions.getVisibility());
    }

    @Test
    public void testActions_showOnlyWhileFocusIsInsideRow() {
        assertTrue(mRow.requestFocus());
        assertEquals(View.VISIBLE, mActions.getVisibility());

        // Focus moving on to the buttons stays there and leaves them showing.
        assertTrue(mPrimaryButton.requestFocus());
        assertTrue(mPrimaryButton.isFocused());
        assertEquals(View.VISIBLE, mActions.getVisibility());
        assertTrue(mSecondaryButton.requestFocus());
        assertTrue(mSecondaryButton.isFocused());
        assertEquals(View.VISIBLE, mActions.getVisibility());

        assertTrue(mOutsideView.requestFocus());
        assertEquals(View.GONE, mActions.getVisibility());

        // So does a row removed while focused, e.g. to be reused.
        assertTrue(mRow.requestFocus());
        assertEquals(View.VISIBLE, mActions.getVisibility());
        mRoot.removeView(mRow);
        assertEquals(View.GONE, mActions.getVisibility());
    }

    @Test
    public void testActions_alwaysVisibleAndTouchableWithoutPrecisionPointer() {
        DeviceInput.setSupportsPrecisionPointerForTesting(false);
        mRow.setHasActions(true);
        ShadowLooper.idleMainLooper();
        assertEquals(View.VISIBLE, mActions.getVisibility());

        // Without a precision pointer, a finger tap reaches the visible button directly.
        Rect button = new Rect(0, 0, mSecondaryButton.getWidth(), mSecondaryButton.getHeight());
        mRow.offsetDescendantRectToMyCoords(mSecondaryButton, button);
        dispatch(
                InputDevice.SOURCE_TOUCHSCREEN,
                MotionEvent.ACTION_DOWN,
                button.centerX(),
                button.centerY());
        dispatch(
                InputDevice.SOURCE_TOUCHSCREEN,
                MotionEvent.ACTION_UP,
                button.centerX(),
                button.centerY());
        assertEquals(1, mSecondaryButtonClicks);

        // Rows without actions still keep the container hidden.
        mRow.setHasActions(false);
        assertEquals(View.GONE, mActions.getVisibility());
    }

    @Test
    public void testRowWithoutActions_neverShowsThem() {
        mRow.setHasActions(false);
        mouse(MotionEvent.ACTION_HOVER_MOVE, 10, mRow.getHeight() / 2f);
        assertTrue(mRow.requestFocus());
        assertEquals(View.GONE, mActions.getVisibility());
    }
}
