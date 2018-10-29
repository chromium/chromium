// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;

/**
 * A location bar implementation specific for smaller/phone screens.
 */
public class LocationBarPhone extends LocationBarLayout {
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;

    private static final int ACTION_BUTTON_TOUCH_OVERFLOW_LEFT = 15;

    private View mFirstVisibleFocusedView;

    private Runnable mKeyboardResizeModeTask;

    /**
     * Constructor used to inflate from XML.
     */
    public LocationBarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFirstVisibleFocusedView = findViewById(R.id.url_bar);

        Rect delegateArea = new Rect();
        mUrlActionContainer.getHitRect(delegateArea);
        delegateArea.left -= ACTION_BUTTON_TOUCH_OVERFLOW_LEFT;
        TouchDelegate touchDelegate = new TouchDelegate(delegateArea, mUrlActionContainer);
        assert mUrlActionContainer.getParent() == this;
        setTouchDelegate(touchDelegate);
    }

    /**
     * @return The first view visible when the location bar is focused.
     */
    public View getFirstViewVisibleWhenFocused() {
        return mFirstVisibleFocusedView;
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;

        if (percent > 0f) {
            mUrlActionContainer.setVisibility(VISIBLE);
        } else if (percent == 0f && !isUrlFocusChangeInProgress()) {
            // If a URL focus change is in progress, then it will handle setting the visibility
            // correctly after it completes.  If done here, it would cause the URL to jump due
            // to a badly timed layout call.
            mUrlActionContainer.setVisibility(GONE);
        }

        updateButtonVisibility();
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            // Remove the focus of this view once the URL field has taken focus as this view no
            // longer needs it.
            setFocusable(false);
            setFocusableInTouchMode(false);
        }
        setUrlFocusChangeInProgress(true);
        super.onUrlFocusChange(hasFocus);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        boolean needsCanvasRestore = false;
        if (child == mUrlBar && mUrlActionContainer.getVisibility() == VISIBLE) {
            canvas.save();

            // Clip the URL bar contents to ensure they do not draw under the URL actions during
            // focus animations.  Based on the RTL state of the location bar, the url actions
            // container can be on the left or right side, so clip accordingly.
            if (mUrlBar.getLeft() < mUrlActionContainer.getLeft()) {
                canvas.clipRect(0, 0, (int) mUrlActionContainer.getX(), getBottom());
            } else {
                canvas.clipRect(mUrlActionContainer.getX() + mUrlActionContainer.getWidth(), 0,
                        getWidth(), getBottom());
            }
            needsCanvasRestore = true;
        }
        boolean retVal = super.drawChild(canvas, child, drawingTime);
        if (needsCanvasRestore) {
            canvas.restore();
        }
        return retVal;
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change.  This will be called after any animations are performed to transition from one
     * focus state to the other.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void finishUrlFocusChange(boolean hasFocus) {
        if (!hasFocus) {
            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            postDelayed(new Runnable() {
                @Override
                public void run() {
                    getWindowAndroid().getKeyboardDelegate().hideKeyboard(mUrlBar);
                }
            }, KEYBOARD_HIDE_DELAY_MS);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE, true);
            mUrlActionContainer.setVisibility(GONE);
        } else {
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN, false);
            getWindowAndroid().getKeyboardDelegate().showKeyboard(mUrlBar);
        }
        setUrlFocusChangeInProgress(false);

        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (hasFocus && ntp != null && ntp.isLocationBarShownInNTP()) {
            updateFadingBackgroundView(true, true);
        }
    }

    @Override
    protected void updateButtonVisibility() {
        super.updateButtonVisibility();
        updateMicButtonVisibility(mUrlFocusChangePercent);
    }

    @Override
    public boolean shouldAnimateIconChanges() {
        return super.shouldAnimateIconChanges() || isUrlFocusChangeInProgress();
    }

    /**
     * @param softInputMode The software input resize mode.
     * @param delay Delay the change in input mode.
     */
    private void setSoftInputMode(final int softInputMode, boolean delay) {
        final WindowDelegate delegate = getWindowDelegate();

        if (mKeyboardResizeModeTask != null) {
            removeCallbacks(mKeyboardResizeModeTask);
            mKeyboardResizeModeTask = null;
        }

        if (delegate == null || delegate.getWindowSoftInputMode() == softInputMode) return;

        if (delay) {
            mKeyboardResizeModeTask = new Runnable() {
                @Override
                public void run() {
                    delegate.setWindowSoftInputMode(softInputMode);
                    mKeyboardResizeModeTask = null;
                }
            };
            postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
        } else {
            delegate.setWindowSoftInputMode(softInputMode);
        }
    }
}
