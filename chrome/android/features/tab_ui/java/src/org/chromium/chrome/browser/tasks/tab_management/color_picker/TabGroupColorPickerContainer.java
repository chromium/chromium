// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.button.MaterialButtonToggleGroup;
import com.google.android.material.shape.ShapeAppearance;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.TabGroupColorPickerCoordinator.TabGroupColorPickerLayoutType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.util.AttrUtils;

import java.util.List;

/** LinearLayout for the tab group specific color picker component. */
@NullMarked
public class TabGroupColorPickerContainer extends MaterialButtonToggleGroup {
    private @Nullable List<View> mColorViews;
    private @TabGroupColorPickerLayoutType int mLayoutType;

    /** Constructs a new tab group color picker. */
    public TabGroupColorPickerContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mColorViews == null || mColorViews.isEmpty()) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        int widthMode = MeasureSpec.getMode(widthMeasureSpec);
        int widthSize = MeasureSpec.getSize(widthMeasureSpec);

        int desiredWidth = getSingleRowWidth();
        if (mLayoutType == TabGroupColorPickerLayoutType.DOUBLE_ROW) {
            desiredWidth = getDoubleRowWidth();
        } else if (mLayoutType == TabGroupColorPickerLayoutType.DYNAMIC) {
            if (widthMode != MeasureSpec.UNSPECIFIED && getSingleRowWidth() > widthSize) {
                desiredWidth = getDoubleRowWidth();
            }
        }

        int forcedWidthMeasureSpec = MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY);
        super.onMeasure(forcedWidthMeasureSpec, heightMeasureSpec);

        restoreChildShapes();
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        restoreChildShapes();
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        restoreChildShapes();
        super.dispatchDraw(canvas);
    }

    private void restoreChildShapes() {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof MaterialButton button) {
                ShapeAppearance original =
                        (ShapeAppearance) button.getTag(R.id.tag_original_shape_appearance);
                if (original != null) {
                    button.setShapeAppearance(original);
                }
            }
        }
    }

    public void setColorViews(List<View> colorViews) {
        mColorViews = colorViews;
        removeAllViews();
        for (View view : mColorViews) {
            addView(view);
        }
    }

    public @TabGroupColorPickerLayoutType int getTabGroupColorPickerLayoutType() {
        return mLayoutType;
    }

    public void setTabGroupColorPickerLayoutType(@TabGroupColorPickerLayoutType int layoutType) {
        mLayoutType = layoutType;
    }

    public int getSingleRowWidth() {
        if (mColorViews == null) return 0;
        return mColorViews.size() * getColorButtonSize() + getPaddingLeft() + getPaddingRight();
    }

    public int getDoubleRowWidth() {
        if (mColorViews == null) return 0;
        return ((mColorViews.size() + 1) / 2) * getColorButtonSize()
                + getPaddingLeft()
                + getPaddingRight();
    }

    private int getColorButtonSize() {
        if (mColorViews == null) return 0;
        return AttrUtils.getDimensionPixelSize(
                mColorViews.get(0).getContext(), R.attr.minInteractTargetSize);
    }
}
