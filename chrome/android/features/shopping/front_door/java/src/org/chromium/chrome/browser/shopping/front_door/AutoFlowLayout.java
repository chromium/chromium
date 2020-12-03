package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import java.util.HashMap;
import java.util.Map;

public class AutoFlowLayout extends LinearLayout {
    private Map<Integer, Integer> mRowHeight = new HashMap<>();

    public AutoFlowLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int width =
                MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
        int height = MeasureSpec.getSize(heightMeasureSpec) - getPaddingTop() - getPaddingBottom();

        int x = getPaddingLeft();
        int y = getPaddingTop();
        int rowHeight = 0;

        int paddingLeft = getPaddingLeft();
        int paddingRight = getPaddingRight();
        int paddingTop = getPaddingTop();
        int paddingBottom = getPaddingBottom();

        final int childCount = getChildCount();

        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);

            if (child.getVisibility() != GONE) {
                final LayoutParams lp = (LayoutParams) child.getLayoutParams();

                final int childWidthMeasureSpec = getChildMeasureSpec(widthMeasureSpec,
                        paddingLeft + paddingRight + lp.leftMargin + lp.rightMargin, lp.width);
                final int childHeightMeasureSpec = getChildMeasureSpec(heightMeasureSpec,
                        paddingTop + paddingBottom + lp.topMargin + lp.bottomMargin, lp.height);
                child.measure(childWidthMeasureSpec, childHeightMeasureSpec);

                final int childWidth = child.getMeasuredWidth();
                final int childHeight = child.getMeasuredHeight();

                if (x + lp.leftMargin + childWidth + lp.rightMargin > width) {
                    x = paddingLeft;
                    y += rowHeight;
                    rowHeight = 0;
                }

                rowHeight = Math.max(rowHeight, lp.topMargin + childHeight + lp.bottomMargin);

                mRowHeight.put(y, rowHeight);

                x += lp.leftMargin + childWidth + lp.rightMargin;
            }
        }

        int heightMeasureMode = MeasureSpec.getMode(heightMeasureSpec);
        if (heightMeasureMode == MeasureSpec.UNSPECIFIED
                || (heightMeasureMode == MeasureSpec.AT_MOST && y + rowHeight < height)) {
            height = y + rowHeight;
        }
        setMeasuredDimension(width, height);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final int childCount = getChildCount();
        final int width = right - left;

        int x = getPaddingLeft();
        int y = getPaddingTop();

        for (int i = 0; i < childCount; i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() != GONE) {
                final int childWidth = child.getMeasuredWidth();
                final int childHeight = child.getMeasuredHeight();
                final LayoutParams lp = (LayoutParams) child.getLayoutParams();
                if (x + lp.leftMargin + childWidth + lp.rightMargin > width) {
                    x = getPaddingLeft();
                    y += mRowHeight.get(y);
                }
                child.layout(x + lp.leftMargin, y + lp.topMargin, x + childWidth + lp.rightMargin,
                        y + childHeight + lp.bottomMargin);
                x += lp.leftMargin + childWidth + lp.rightMargin;
            }
        }
    }
}
