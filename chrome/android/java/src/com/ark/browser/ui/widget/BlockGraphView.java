package com.ark.browser.ui.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

import com.zpj.skin.SkinEngine;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.R;

public class BlockGraphView extends View {
    private static int BLOCKS_PER_LINE = 20;

    private int mForegroundColor, mBackgroundColor;
    private float mBlockSize;
    private int mLineCount;
    private DownloadItem mMission;

    private final Paint p;

    public BlockGraphView(Context context) {
        this(context, null);
    }

    public BlockGraphView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BlockGraphView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        mBackgroundColor = SkinEngine.getColor(context, R.attr.backgroundColorAccent);
        mForegroundColor = ApiCompatibilityUtils.getColor(getResources(), R.color.google_blue_500);

        p = new Paint();
        p.setFlags(Paint.ANTI_ALIAS_FLAG);
    }

    public void setMission(DownloadItem mission) {
        mMission = mission;
        setWillNotDraw(false);
    }

    public void setForegroundColor(int mForegroundColor) {
        this.mForegroundColor = mForegroundColor;
    }

    public void setBackgroundColor(int mBackgroundColor) {
        this.mBackgroundColor = mBackgroundColor;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        mBlockSize = (float) width / BLOCKS_PER_LINE - 1;
        mLineCount = 10; // (int) Math.ceil((double) mMission.getBlocks() / BLOCKS_PER_LINE);
        float height = mLineCount * (mBlockSize + 1);
        setMeasuredDimension(width, (int) height);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        for (int i = 0; i < mLineCount; i++) {
            for (int j = 0; j < BLOCKS_PER_LINE; j++) {
                long pos = i * BLOCKS_PER_LINE + j;
//                if (pos >= mMission.getBlocks()) {
//                    break;
//                }

                if (i < 5) { // mMission.isBlockPreserved(pos)
                    p.setColor(mForegroundColor);
                } else {
                    p.setColor(mBackgroundColor);
                }

                float left = (mBlockSize + 1) * j;
                float right = left + mBlockSize;
                float top = (mBlockSize + 1) * i;
                float bottom = top + mBlockSize;
                canvas.drawRect(left, top, right, bottom, p);
            }
        }
    }
}

