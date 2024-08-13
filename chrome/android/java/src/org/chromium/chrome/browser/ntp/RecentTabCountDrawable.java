// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Paint.Align;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.DrawableWrapper;
import android.text.TextPaint;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;

import java.util.Locale;

/**
 * Class for drawing a tab count icon on the Recent Tabs Page for bulk tab closures.
 *
 * Loosely based on {@link TabSwitcherDrawable} and modified to handle an SVG asset.
 */
public class RecentTabCountDrawable extends DrawableWrapper {
    // Avoid allocations during draw by pre-allocating a rect.
    private final Rect mTextBounds = new Rect();
    private final TextPaint mTextPaint;
    private ColorStateList mTint;

    private int mTabCount;

    /**
     * Creates a recent tab count icon. A stacked squircle with a number inside for a count.
     * @param context The context for getting resources.
     */
    public RecentTabCountDrawable(Context context) {
        super(
                UiUtils.getTintedDrawable(
                        context,
                        R.drawable.ic_recent_tabs_bulk_20dp,
                        R.color.default_icon_color_tint_list));

        mTextPaint = new TextPaint();
        setTint(
                AppCompatResources.getColorStateList(
                        context, R.color.default_icon_color_tint_list));

        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Align.CENTER);
        mTextPaint.setTypeface(Typeface.create("google-sans-medium", Typeface.BOLD));
        mTextPaint.setColor(getColorForState());
        mTextPaint.setTextSize(
                context.getResources().getDimension(R.dimen.recent_tabs_count_text_size));
    }

    /**
     * Update the count value inside the icon.
     * @param tabCount the number to display in the icon.
     */
    public void updateTabCount(int tabCount) {
        if (tabCount == mTabCount) return;
        mTabCount = tabCount;
        invalidateSelf();
    }

    public void setTint(ColorStateList tint) {
        if (mTint == tint) return;
        mTint = tint;
        updateTintColor();
        super.setTint(getColorForState());
        if (mTextPaint != null) mTextPaint.setColor(getColorForState());
    }

    // DrawableWrapper implementation.
    @Override
    public void setTint(int tint) {
        setTint(ColorStateList.valueOf(tint));
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        // Add text string.
        String textString = getTabCountString();
        if (!textString.isEmpty()) {
            mTextPaint.getTextBounds(textString, 0, textString.length(), mTextBounds);
            Rect bounds = super.getBounds();
            // Constants are based on X/Y position in the icon from the redlines the UX designer
            // provided.
            final int textX = bounds.left + Math.round(0.583f * bounds.width());
            final int textY =
                    bounds.top
                            + Math.round(14.0f / 24.0f * bounds.height())
                            + (mTextBounds.bottom - mTextBounds.top) / 2
                            - mTextBounds.bottom;

            canvas.drawText(textString, textX, textY, mTextPaint);
        }
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean ret = updateTintColor();
        if (ret) mTextPaint.setColor(getColorForState());
        return ret || super.onStateChange(state);
    }

    @Override
    public boolean isStateful() {
        return true;
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setAlpha(int alpha) {
        super.setAlpha(alpha);
        mTextPaint.setAlpha(alpha);
    }

    private boolean updateTintColor() {
        if (mTint == null) return false;
        setColorFilter(getColorForState(), PorterDuff.Mode.SRC_IN);
        return true;
    }

    private String getTabCountString() {
        if (mTabCount <= 0) {
            return "";
        } else if (mTabCount > 99) {
            return ":D";
        } else {
            return String.format(Locale.getDefault(), "%d", mTabCount);
        }
    }

    private int getColorForState() {
        return mTint.getColorForState(getState(), 0);
    }
}
