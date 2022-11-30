// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Picture;
import android.graphics.PorterDuff.Mode;
import android.graphics.Rect;
import android.graphics.RectF;

/**
 * Canvas implementation that does not do any rendering, and throws an exception when anything tries
 * to render anything using it.
 */
public class NoopCanvas extends Canvas {
    /**
     *  Thrown when the Canvas would have drawn something.
     */
    public static class NoopException extends RuntimeException {}

    private NoopException mException;

    /**
     * @param bitmap The bitmap to create the Canvas from.
     * @param preGenerateException If true, optimize performance by pre-generating the exception to
     *        be thrown. This will make the exceptions stack trace meaningless.
     */
    NoopCanvas(Bitmap bitmap, boolean preGenerateException) {
        super(bitmap);
        // We promised not to render anything, so recycling the bitmap should be safe :)
        bitmap.recycle();
        if (preGenerateException) {
            mException = new NoopException() {
                @Override
                public synchronized Throwable fillInStackTrace() {
                    // Short-circuit stack trace generation since we don't care about it.
                    return this;
                }
            };
        }
    }

    @Override
    public void drawRGB(int r, int g, int b) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawARGB(int a, int r, int g, int b) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawColor(int color) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawColor(int color, Mode mode) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPaint(Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPoints(float[] pts, int offset, int count, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPoints(float[] pts, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPoint(float x, float y, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawLine(float startX, float startY, float stopX, float stopY, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawLines(float[] pts, int offset, int count, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawLines(float[] pts, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawRect(RectF rect, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawRect(Rect r, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawRect(float left, float top, float right, float bottom, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawOval(RectF oval, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawOval(float left, float top, float right, float bottom, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawCircle(float cx, float cy, float radius, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawArc(
            RectF oval, float startAngle, float sweepAngle, boolean useCenter, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawArc(float left, float top, float right, float bottom, float startAngle,
            float sweepAngle, boolean useCenter, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawRoundRect(RectF rect, float rx, float ry, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawRoundRect(
            float left, float top, float right, float bottom, float rx, float ry, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPath(Path path, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(Bitmap bitmap, float left, float top, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(Bitmap bitmap, Rect src, RectF dst, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(Bitmap bitmap, Rect src, Rect dst, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(int[] colors, int offset, int stride, float x, float y, int width,
            int height, boolean hasAlpha, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(int[] colors, int offset, int stride, int x, int y, int width,
            int height, boolean hasAlpha, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmap(Bitmap bitmap, Matrix matrix, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawBitmapMesh(Bitmap bitmap, int meshWidth, int meshHeight, float[] verts,
            int vertOffset, int[] colors, int colorOffset, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawVertices(VertexMode mode, int vertexCount, float[] verts, int vertOffset,
            float[] texs, int texOffset, int[] colors, int colorOffset, short[] indices,
            int indexOffset, int indexCount, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawText(char[] text, int index, int count, float x, float y, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawText(String text, float x, float y, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawText(String text, int start, int end, float x, float y, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawText(CharSequence text, int start, int end, float x, float y, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawTextRun(char[] text, int index, int count, int contextIndex, int contextCount,
            float x, float y, boolean isRtl, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawTextRun(CharSequence text, int start, int end, int contextStart, int contextEnd,
            float x, float y, boolean isRtl, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPosText(char[] text, int index, int count, float[] pos, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPosText(String text, float[] pos, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawTextOnPath(char[] text, int index, int count, Path path, float hOffset,
            float vOffset, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawTextOnPath(String text, Path path, float hOffset, float vOffset, Paint paint) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPicture(Picture picture) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPicture(Picture picture, RectF dst) {
        throw(mException == null) ? new NoopException() : mException;
    }

    @Override
    public void drawPicture(Picture picture, Rect dst) {
        throw(mException == null) ? new NoopException() : mException;
    }
}
