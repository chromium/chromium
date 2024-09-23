// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/** LinearLayout for the tab group specific color picker component. */
public class TabGroupColorPickerContainer extends ColorPickerContainer {
    private final LinearLayout.LayoutParams mParams;
    private Boolean mIsDoubleRow;
    private boolean mSkipOnMeasure;
    // The following variables become @NonNull post-inflation, before the UI is shown.
    private List<FrameLayout> mColorViews;
    private LinearLayout mFirstRow;
    private LinearLayout mSecondRow;
    private @ColorPickerLayoutType int mLayoutType;

    /** Constructs a new tab group color picker. */
    public TabGroupColorPickerContainer(Context context, AttributeSet attrs) {
        super(context, attrs);

        mParams =
                new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        mParams.gravity = Gravity.CENTER;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFirstRow = findViewById(R.id.color_picker_first_row);
        mSecondRow = findViewById(R.id.color_picker_second_row);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Skip additional onMeasure computations if child views are being added.
        if (mSkipOnMeasure) {
            return;
        }
        mSkipOnMeasure = true;

        assert mColorViews != null;
        if (mLayoutType == ColorPickerLayoutType.DYNAMIC) {
            // If the color items exceed the width of the container, split into two rows.
            if (mColorViews.get(0).getMeasuredWidth() * mColorViews.size() > getMeasuredWidth()) {
                // If the current setup is a single row, perform a re-layout to a double row.
                if (Boolean.FALSE.equals(mIsDoubleRow)) {
                    addColorsToDoubleRow();
                }
            } else {
                // If the current setup is a double row or the boolean value is null (initial pass)
                // then perform a re-layout to a single row.
                boolean isDoubleRowOrInitialPass = !Boolean.FALSE.equals(mIsDoubleRow);
                if (isDoubleRowOrInitialPass) {
                    addColorsToSingleRow();
                }
            }
        } else if (mLayoutType == ColorPickerLayoutType.DOUBLE_ROW) {
            addColorsToDoubleRow();
        } else {
            addColorsToSingleRow();
        }

        // Re-measure the color items in the color palette and reset the skip boolean.
        mSkipOnMeasure = false;
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void setColorViews(List<FrameLayout> colorViews) {
        mColorViews = colorViews;
    }

    @Override
    public void setColorPickerLayoutType(@ColorPickerLayoutType int layoutType) {
        mLayoutType = layoutType;
    }

    private void addColorsToSingleRow() {
        mFirstRow.removeAllViews();
        mSecondRow.removeAllViews();

        for (FrameLayout view : mColorViews) {
            mFirstRow.addView(view, mParams);
        }
        mIsDoubleRow = false;
    }

    private void addColorsToDoubleRow() {
        mFirstRow.removeAllViews();
        mSecondRow.removeAllViews();

        for (int i = 0; i < mColorViews.size(); i++) {
            if (i < (mColorViews.size() + 1) / 2) {
                mFirstRow.addView(mColorViews.get(i), mParams);
            } else {
                mSecondRow.addView(mColorViews.get(i), mParams);
            }
        }
        mIsDoubleRow = true;
    }
}
