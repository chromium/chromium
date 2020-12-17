// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;

/**
 * A special linear layout that limits its maximum size to always stay below the Chrome navigation
 * bar.
 */
public class AssistantRootViewContainer
        extends LinearLayout implements BrowserControlsStateProvider.Observer {
    private final ChromeActivity mActivity;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private Rect mVisibleViewportRect = new Rect();
    private float mTalkbackSheetSizeFraction;
    private boolean mTalkbackResizingDisabled;

    public AssistantRootViewContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        Activity activity = ContextUtils.activityFromContext(context);
        assert activity instanceof ChromeActivity;
        mActivity = (ChromeActivity) activity;
        mBrowserControlsStateProvider = mActivity.getBrowserControlsManager();
        mBrowserControlsStateProvider.addObserver(this);
    }

    public void setTalkbackViewSizeFraction(float fraction) {
        mTalkbackSheetSizeFraction = fraction;
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        invalidate();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        invalidate();
    }

    public void disableTalkbackViewResizing() {
        mTalkbackResizingDisabled = true;
    }

    void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
        int availableHeight = mVisibleViewportRect.height()
                - mBrowserControlsStateProvider.getContentOffset()
                - mBrowserControlsStateProvider.getBottomControlsHeight()
                - mBrowserControlsStateProvider.getBottomControlOffset();

        int targetHeight;
        int mode;
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled() && !mTalkbackResizingDisabled) {
            // TODO(b/143944870): Make this more stable with landscape mode.
            targetHeight = (int) (availableHeight * mTalkbackSheetSizeFraction);
            mode = MeasureSpec.EXACTLY;
        } else {
            targetHeight = Math.min(MeasureSpec.getSize(heightMeasureSpec), availableHeight);
            mode = MeasureSpec.AT_MOST;
        }
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(targetHeight, mode));
    }
}
