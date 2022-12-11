package com.ark.browser.ui.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.Nullable;

import com.zpj.utils.ScreenUtils;

public class FitWidthImageView extends View {

    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Matrix matrix = new Matrix();
    private final float mRadio;
    private Bitmap bitmap;

    public FitWidthImageView(Context context) {
        this(context, null);
    }

    public FitWidthImageView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public FitWidthImageView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        mRadio = 1f / (1f + ScreenUtils.dp2px(context, 42) / ScreenUtils.getStatusBarHeight(context));
    }

    public void setImageBitmap(Bitmap bitmap) {
        this.bitmap = bitmap;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (bitmap != null) {
            matrix.reset();
            float scale = (float) getWidth() / bitmap.getWidth();
            matrix.setScale(scale, scale);
//            matrix.postTranslate(0f, (int) ((getHeight() - scale * bitmap.getHeight()) / 2f));
            matrix.postTranslate(0f, (getHeight() - scale * bitmap.getHeight()) * mRadio);
            canvas.drawBitmap(bitmap, matrix, mPaint);
        }
    }
}

