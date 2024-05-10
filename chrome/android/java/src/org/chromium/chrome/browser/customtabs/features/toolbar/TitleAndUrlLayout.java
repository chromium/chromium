// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Px;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;

/**
 * A specialized {@link FrameLayout} that wraps the title and URL bars. It currently has 2 purposes:
 * - Prevents its children from getting touch events. This is especially useful to prevent
 *   {@link UrlBar} from running custom touch logic since it is read-only in custom tabs.
 * - Scales down the text within if they are overlapping. This can happen if the system font size
 *   setting is set to a large value, e.g. 200%.
 */
class TitleAndUrlLayout extends FrameLayout {
    private final GestureDetector mGestureDetector;
    private TextView mTitleBar;
    private UrlBar mUrlBar;
    // Bit to make sure we scale and re-measure the text only once per activity launch.
    private boolean mTextScaled;

    public TitleAndUrlLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mGestureDetector =
                new GestureDetector(
                        getContext(),
                        new GestureDetector.SimpleOnGestureListener() {
                            @Override
                            public boolean onSingleTapConfirmed(MotionEvent e) {
                                if (LibraryLoader.getInstance().isInitialized()) {
                                    RecordUserAction.record("CustomTabs.TapUrlBar");
                                }
                                return super.onSingleTapConfirmed(e);
                            }
                        },
                        ThreadUtils.getUiThreadHandler());
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleBar = findViewById(R.id.title_bar);
        mUrlBar = findViewById(R.id.url_bar);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int titleHeight = mTitleBar.getMeasuredHeight();
        int urlHeight = mUrlBar.getMeasuredHeight();
        if (!mTextScaled
                && titleHeight > 0
                && urlHeight > 0
                && (titleHeight + urlHeight > getMeasuredHeight())) {
            float titleToTotalRatio =
                    mTitleBar.getTextSize() / (mTitleBar.getTextSize() + mUrlBar.getTextSize());
            int titleDesiredHeight = Math.round(getMeasuredHeight() * titleToTotalRatio);
            int urlDesiredHeight = Math.round(getMeasuredHeight() * (1 - titleToTotalRatio));
            scaleDownText(mTitleBar, titleDesiredHeight);
            scaleDownText(mUrlBar, urlDesiredHeight);
            ((FrameLayout.LayoutParams) mTitleBar.getLayoutParams()).bottomMargin =
                    urlDesiredHeight;
            mTextScaled = true;

            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    private static void scaleDownText(TextView textView, int desiredHeight) {
        float fontHeight = getMaxHeightOfFont(textView.getPaint().getFontMetrics());
        float scaleRatio = desiredHeight / fontHeight;
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, textView.getTextSize() * scaleRatio);
    }

    private static @Px float getMaxHeightOfFont(Paint.FontMetrics fontMetrics) {
        return fontMetrics.bottom - fontMetrics.top;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return true;
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        mGestureDetector.onTouchEvent(event);
        return super.onTouchEvent(event);
    }
}
