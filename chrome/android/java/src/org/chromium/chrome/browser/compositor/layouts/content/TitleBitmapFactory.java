// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.os.SystemClock;
import android.text.Layout;
import android.text.TextPaint;
import android.text.TextUtils;
import android.util.Log;
import android.view.InflateException;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.util.StyleUtils;

/** A factory that creates text and favicon bitmaps. */
public class TitleBitmapFactory {
    private static final String TAG = "TitleBitmapFactory";

    private static final float TITLE_WIDTH_PERCENTAGE = 1.f;
    // Canvas#drawText() seems to fail when trying to draw 4100 or more characters.
    // See https://crbug.com/524390/ for more details.
    private static final int MAX_NUM_TITLE_CHAR = 1000;

    // We were drawing up to 1000 characters, but only displaying ~30 in the tab strip. Experiment
    // with a smaller limit.
    private static final int SMALLER_MAX_NUM_TITLE_CHAR = 100;

    private final int mMaxWidth;
    private final int mViewHeight;
    private int mFaviconDimension;
    private boolean mIncognito;

    private final TextPaint mTabTextPaint;
    private final float mTabTextHeight;
    private final float mTabTextYOffset;

    private final TextPaint mGroupTextPaint;
    private final int mGroupTextHeight;
    private final float mGroupTextYOffset;

    /**
     * @param context The current Android's context.
     * @param incognito Whether the title are for incognito mode.
     */
    public TitleBitmapFactory(Context context, boolean incognito) {
        Resources res = context.getResources();
        mIncognito = incognito;

        boolean fakeBoldText = res.getBoolean(R.bool.compositor_tab_title_fake_bold_text);

        mTabTextPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        if (mIncognito) {
            int incognitoTabTextColor =
                    AppCompatResources.getColorStateList(
                                    context, R.color.compositor_tab_title_bar_text_incognito)
                            .getDefaultColor();
            mTabTextPaint.setColor(incognitoTabTextColor);
        }
        StyleUtils.applyTextAppearanceToTextPaint(
                context,
                mTabTextPaint,
                R.style.TextAppearance_TextMedium_Primary,
                /* applyFontFamily= */ true,
                /* applyTextSize= */ true,
                !mIncognito);
        mTabTextPaint.setFakeBoldText(fakeBoldText);
        mTabTextPaint.density = res.getDisplayMetrics().density;
        FontMetrics tabTextFontMetrics = mTabTextPaint.getFontMetrics();
        mTabTextHeight = (float) Math.ceil(tabTextFontMetrics.bottom - tabTextFontMetrics.top);
        mTabTextYOffset = -tabTextFontMetrics.top;

        mGroupTextPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        StyleUtils.applyTextAppearanceToTextPaint(
                context,
                mGroupTextPaint,
                R.style.TextAppearance_TextSmall,
                /* applyFontFamily */ true,
                /* applyTextSize= */ true,
                /* applyTextColor= */ false);
        mGroupTextPaint.setFakeBoldText(fakeBoldText);
        mGroupTextPaint.density = res.getDisplayMetrics().density;
        FontMetrics groupTextFontMetrics = mGroupTextPaint.getFontMetrics();
        mGroupTextHeight = (int) Math.ceil(groupTextFontMetrics.bottom - groupTextFontMetrics.top);
        mGroupTextYOffset = -groupTextFontMetrics.top;

        mFaviconDimension = res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mViewHeight = (int) Math.max(mFaviconDimension, mTabTextHeight);

        int width = res.getDisplayMetrics().widthPixels;
        int height = res.getDisplayMetrics().heightPixels;
        mMaxWidth = (int) (TITLE_WIDTH_PERCENTAGE * Math.max(width, height));

        // Set the favicon dimension here.
        mFaviconDimension = Math.min(mMaxWidth, mFaviconDimension);
    }

    /**
     * Generates the favicon bitmap.
     *
     * @param favicon   The favicon of the tab.
     * @return          The Bitmap with the favicon.
     */
    public Bitmap getFaviconBitmap(Bitmap favicon) {
        assert favicon != null;
        try {
            Bitmap b =
                    Bitmap.createBitmap(
                            mFaviconDimension, mFaviconDimension, Bitmap.Config.ARGB_8888);
            Canvas c = new Canvas(b);
            if (favicon.getWidth() > mFaviconDimension || favicon.getHeight() > mFaviconDimension) {
                float scale =
                        (float) mFaviconDimension
                                / Math.max(favicon.getWidth(), favicon.getHeight());
                c.scale(scale, scale);
            } else {
                c.translate(
                        Math.round((mFaviconDimension - favicon.getWidth()) / 2.0f),
                        Math.round((mFaviconDimension - favicon.getHeight()) / 2.0f));
            }
            c.drawBitmap(favicon, 0, 0, null);
            return b;
        } catch (OutOfMemoryError ex) {
            Log.e(TAG, "OutOfMemoryError while building favicon texture.");
        } catch (InflateException ex) {
            Log.w(TAG, "InflateException while building favicon texture.");
        }

        return null;
    }

    /**
     * Generates the tab title bitmap.
     *
     * @param title The title of the group.
     * @return The Bitmap with the title.
     */
    public Bitmap getTabTitleBitmap(String title) {
        return getTitleBitmap(mTabTextPaint, mTabTextHeight, mTabTextYOffset, title);
    }

    /**
     * Generates the group title bitmap.
     *
     * @param filter To fetch tab information from.
     * @param context The current Android's context.
     * @param rootId The root ID of the group.
     * @param title The title of the group.
     * @return The Bitmap with the title.
     */
    public Bitmap getGroupTitleBitmap(
            TabGroupModelFilter filter, Context context, int rootId, String title) {
        @TabGroupColorId int colorId = filter.getTabGroupColor(rootId);
        @ColorInt
        int color =
                ColorPickerUtils.getTabGroupColorPickerItemTextColor(context, colorId, mIncognito);
        mGroupTextPaint.setColor(color);
        return getTitleBitmap(mGroupTextPaint, mGroupTextHeight, mGroupTextYOffset, title);
    }

    /**
     * Generates a title bitmap.
     *
     * @param textPaint The TextPaint used to create the title bitmap.
     * @param height The height of the title.
     * @param yOffset The y-offset of the title.
     * @param title The title of the tab.
     * @return The Bitmap with the title.
     */
    public Bitmap getTitleBitmap(TextPaint textPaint, float height, float yOffset, String title) {
        try {
            final long startTime = SystemClock.elapsedRealtime();
            boolean drawText = !TextUtils.isEmpty(title);
            int textWidth = drawText ? getTitleWidth(title, textPaint) : 0;

            // Minimum 1 width bitmap to avoid createBitmap function's IllegalArgumentException,
            // when textWidth == 0.
            Bitmap b =
                    Bitmap.createBitmap(
                            Math.max(Math.min(mMaxWidth, textWidth), 1),
                            mViewHeight,
                            Bitmap.Config.ARGB_8888);
            Canvas c = new Canvas(b);
            if (drawText) {
                final int maxCharsToDraw =
                        ChromeFeatureList.sSmallerTabStripTitleLimit.isEnabled()
                                ? SMALLER_MAX_NUM_TITLE_CHAR
                                : MAX_NUM_TITLE_CHAR;
                RecordHistogram.recordCount100Histogram(
                        "Android.TabStrip.TitleBitmapFactory.getTitleBitmap.Length",
                        title.length());
                c.drawText(
                        title,
                        0,
                        Math.min(maxCharsToDraw, title.length()),
                        0,
                        Math.round((mViewHeight - height) / 2.0f + yOffset),
                        textPaint);
            }

            RecordHistogram.recordTimesHistogram(
                    "Android.TabStrip.TitleBitmapFactory.getTitleBitmap.Duration",
                    SystemClock.elapsedRealtime() - startTime);

            return b;
        } catch (OutOfMemoryError ex) {
            Log.e(TAG, "OutOfMemoryError while building title texture.");
        } catch (InflateException ex) {
            Log.w(TAG, "InflateException while building title texture.");
        }

        return null;
    }

    /**
     * @param titleString The title of the tab group.
     * @return The width in px of the title.
     */
    public int getGroupTitleWidth(String titleString) {
        return getTitleWidth(titleString, mGroupTextPaint);
    }

    private int getTitleWidth(String titleString, TextPaint textPaint) {
        return (int) Math.ceil(Layout.getDesiredWidth(titleString, textPaint));
    }
}
