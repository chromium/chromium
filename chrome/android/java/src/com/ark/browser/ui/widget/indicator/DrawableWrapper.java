package com.ark.browser.ui.widget.indicator;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;

/**
 * @author cenxiaozhong
 * @date 2018/2/23
 * @since 1.0.0
 */
public class DrawableWrapper extends Drawable {

    private final Drawable mWrapped;

    public DrawableWrapper(@NonNull Drawable drawable) {
        mWrapped = drawable;
    }

    public Drawable getWrappedDrawable() {
        return mWrapped;
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        mWrapped.draw(canvas);
    }

    @Override
    public int getChangingConfigurations() {
        return mWrapped.getChangingConfigurations();
    }

    @Override
    public ConstantState getConstantState() {
        return mWrapped.getConstantState();
    }

    @Override
    public Drawable getCurrent() {
        return mWrapped.getCurrent();
    }

    @Override
    public int getIntrinsicHeight() {
        return mWrapped.getIntrinsicHeight();
    }

    @Override
    public int getIntrinsicWidth() {
        return mWrapped.getIntrinsicWidth();
    }

    @Override
    public int getMinimumHeight() {
        return mWrapped.getMinimumHeight();
    }

    @Override
    public int getMinimumWidth() {
        return mWrapped.getMinimumWidth();
    }

    @Override
    public int getOpacity() {
        return mWrapped.getOpacity();
    }

    @Override
    public boolean getPadding(Rect padding) {
        return mWrapped.getPadding(padding);
    }

    @Override
    public int[] getState() {
        return mWrapped.getState();
    }

    @Override
    public Region getTransparentRegion() {
        return mWrapped.getTransparentRegion();
    }

    @Override
    public void inflate(Resources r, XmlPullParser parser, AttributeSet attrs)
            throws XmlPullParserException, IOException {
        mWrapped.inflate(r, parser, attrs);
    }

    @Override
    public boolean isStateful() {
        return mWrapped.isStateful();
    }

    @Override
    public void jumpToCurrentState() {
        mWrapped.jumpToCurrentState();
    }

    @Override
    public Drawable mutate() {
        return mWrapped.mutate();
    }

    @Override
    public void setAlpha(@IntRange(from = 0, to = 255) int i) {
        mWrapped.setAlpha(i);
    }

    @Override
    public void scheduleSelf(Runnable what, long when) {
        mWrapped.scheduleSelf(what, when);
    }

    @Override
    public void setChangingConfigurations(int configs) {
        mWrapped.setChangingConfigurations(configs);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mWrapped.setColorFilter(colorFilter);
    }

    @Override
    public void setColorFilter(int color, PorterDuff.Mode mode) {
        mWrapped.setColorFilter(color, mode);
    }

    @Override
    public void setFilterBitmap(boolean filter) {
        mWrapped.setFilterBitmap(filter);
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        return mWrapped.setVisible(visible, restart);
    }

    @Override
    public void unscheduleSelf(Runnable what) {
        mWrapped.unscheduleSelf(what);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        mWrapped.setBounds(bounds);
    }

    @Override
    protected boolean onLevelChange(int level) {
        return mWrapped.setLevel(level);
    }

    @Override
    protected boolean onStateChange(int[] state) {
        return mWrapped.setState(state);
    }
}

