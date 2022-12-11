package com.ark.browser.ui.drawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

import org.chromium.base.Log;

public class CircleDrawable extends Drawable {
    /**
     * 显示图片
     */
    private Bitmap mBitmap;
    /**
     * BitmapShader
     */
    private BitmapShader mBitmapShader;
    /**
     * 画笔
     */
    private Paint mPaint;
    /**
     * 圆心
     */
    private float cx, cy;
    /**
     * 半径
     */
    private float radius;

    private float size;

    public static Bitmap DrawableToBitmap(Drawable drawable) {

        // 获取 drawable 长宽
        int width = drawable.getIntrinsicWidth();
        int heigh = drawable.getIntrinsicHeight();

        drawable.setBounds(0, 0, width, heigh);

        // 获取drawable的颜色格式
        Bitmap.Config config = drawable.getOpacity() != PixelFormat.OPAQUE ? Bitmap.Config.ARGB_8888
                : Bitmap.Config.RGB_565;
        // 创建bitmap
        Bitmap bitmap = Bitmap.createBitmap(width, heigh, config);
        // 创建bitmap画布
        Canvas canvas = new Canvas(bitmap);
        // 将drawable 内容画到画布中
        drawable.draw(canvas);
        return bitmap;
    }

    public CircleDrawable(Context context, Bitmap bitmap) {
        this.mBitmap = bitmap;
        mBitmapShader = new BitmapShader(mBitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);

        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setShader(mBitmapShader);


//        size = ViewUnit.dp2px(context, Math.min(mBitmap.getWidth(), mBitmap.getHeight()));
        size = mBitmap.getWidth();
        Log.d("CircleDrawable", "size=" + size);

        cx = size / 2;
        cy = cx;
        radius = cx;
    }

    public CircleDrawable(Bitmap bitmap, int size) {
        this.mBitmap = bitmap;
        mBitmapShader = new BitmapShader(mBitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);

        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mPaint.setShader(mBitmapShader);


//        size = ViewUnit.dp2px(context, Math.min(mBitmap.getWidth(), mBitmap.getHeight()));
        this.size = size;
        Log.d("CircleDrawable", "size=" + size);

        cx = size / 2;
        cy = cx;
        radius = cx;
    }

    public CircleDrawable(Context context, Drawable drawable) {
        this(context, DrawableToBitmap(drawable));

//        mBitmapShader = new BitmapShader(mBitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
//
//        mPaint = new Paint();
//        mPaint.setAntiAlias(true);
//        mPaint.setShader(mBitmapShader);
//
//
//        size = ViewUnit.dp2px(Math.min(mBitmap.getWidth(), mBitmap.getHeight()));
//        mBitmap.
//        Log.d("CircleDrawable", "size=" + size);
//
//        cx = size / 2;
//        cy = cx;
//        radius = cx;
    }

    @Override
    public void draw(Canvas canvas) {
//        final Rect rect = new Rect(0, 0, width, heigh);
//        final RectF rectf = new RectF(rect);
//        canvas.drawRoundRect(rectf, );
        canvas.drawCircle(cx, cy, radius, mPaint);
    }

    @Override
    public int getIntrinsicHeight() {
        return (int) size;
    }

    @Override
    public int getIntrinsicWidth() {
        return (int) size;
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}

