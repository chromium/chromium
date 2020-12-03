package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

public class RoundedCornerSquareImageView extends RoundedCornerImageView {
    public RoundedCornerSquareImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        int measuredWidth = getMeasuredWidth();
        setMeasuredDimension(measuredWidth, measuredWidth);
    }
}
