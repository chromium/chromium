package com.ark.browser.ui.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.ColorInt;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.chrome.R;

public class DrawableTintEditText extends EmptyAlertEditText {

    private int drawableTintColor;

    public DrawableTintEditText(Context context) {
        this(context, null);
    }

    public DrawableTintEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray typedArray = context.obtainStyledAttributes(attrs, R.styleable.DrawableTintTextView);
        drawableTintColor = typedArray.getColor(R.styleable.DrawableTintTextView_drawable_tint_color, Color.TRANSPARENT);
        typedArray.recycle();
        tintDrawables();
    }

    public void setDrawableTintColor(int drawableTintColor) {
        this.drawableTintColor = drawableTintColor;
        tintDrawables();
    }

    private void tintDrawables() {
        if (drawableTintColor != Color.TRANSPARENT) {
            Drawable[] drawables = getCompoundDrawablesRelative();
            for (int i = 0; i < drawables.length; i++) {
                Drawable drawable = drawables[i];
                if (drawable != null) {
                    final Drawable wrappedDrawable = DrawableCompat.wrap(drawable);
                    DrawableCompat.setTintList(wrappedDrawable, ColorStateList.valueOf(drawableTintColor));
                    drawables[i] = wrappedDrawable;
                }
            }
            setCompoundDrawablesRelativeWithIntrinsicBounds(drawables[0], drawables[1], drawables[2], drawables[3]);
        }
    }

    public void setTint(@ColorInt int color) {
        setDrawableTintColor(color);
        setTextColor(color);
    }
}

