package org.chromium.chrome.browser.shopping.front_door;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

// Scrim view is adjustable based on parent's dimension.
public class AdjustableScrimView extends LinearLayout {
    private static final float DEFAULT_SCRIM_HEIGHT_RATIO = 0.33F;
    private float mScrimHeightRatio = DEFAULT_SCRIM_HEIGHT_RATIO;

    public AdjustableScrimView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public void setHeightRatio(float ratio) {
        mScrimHeightRatio = ratio;
    }

    // @Override
    // protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
    //     super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    //
    //     int parentWidth = MeasureSpec.getSize(widthMeasureSpec);
    //     int parentHeight = MeasureSpec.getSize(heightMeasureSpec);
    //     this.setMeasuredDimension(parentWidth, (int) (parentHeight * mScrimHeightRatio * 1.0));
    // }
}
