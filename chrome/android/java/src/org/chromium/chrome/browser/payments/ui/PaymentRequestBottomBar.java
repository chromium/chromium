// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.R;

/** This class represents a bar to display at the bottom of the payment request UI. */
public class PaymentRequestBottomBar extends ViewGroup {
    private ImageView mLogoWithName;
    private ImageView mLogo;
    private View mPrimaryButton;
    private View mSecondaryButton;
    private View mSpace;

    /** Constructor for when the PaymentRequestBottomBar is inflated from XML. */
    public PaymentRequestBottomBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLogoWithName = findViewById(R.id.logo_name);
        // Not doing it in the template because the //components payment_request_bottom_bar.xml
        // cannot depend on the //chrome logo.png.
        mLogoWithName.setImageResource(R.drawable.product_logo_name);
        mLogo = findViewById(R.id.logo);
        // Not doing it in the template because the //components template
        // payment_request_bottom_bar.xml cannot depend on the //chrome fre_product_logo.png.
        mLogo.setImageResource(R.drawable.fre_product_logo);
        mPrimaryButton = findViewById(R.id.button_primary);
        mSecondaryButton = findViewById(R.id.button_secondary);
        mSpace = findViewById(R.id.space);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Views layout_width must be set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;

        measureChild(mLogoWithName, widthMeasureSpec, heightMeasureSpec);
        int logoWithNameWidth = mLogoWithName.getMeasuredWidth();

        measureChild(mLogo, widthMeasureSpec, heightMeasureSpec);
        int logoWidth = mLogo.getMeasuredWidth();

        measureChild(mPrimaryButton, widthMeasureSpec, heightMeasureSpec);
        int primaryButtonWidth = mPrimaryButton.getMeasuredWidth();

        measureChild(mSecondaryButton, widthMeasureSpec, heightMeasureSpec);
        int secondaryButtonWidth = mSecondaryButton.getMeasuredWidth();

        MarginLayoutParams layoutParams = (MarginLayoutParams) getLayoutParams();
        int totalWidthWithoutLogo =
                primaryButtonWidth
                        + secondaryButtonWidth
                        + getPaddingLeft()
                        + getPaddingRight()
                        + layoutParams.leftMargin
                        + layoutParams.rightMargin;

        int blankSpaceWidth;
        int maxTotalWidth = MeasureSpec.getSize(widthMeasureSpec);
        if (totalWidthWithoutLogo + logoWithNameWidth <= maxTotalWidth) {
            mLogo.setVisibility(View.GONE);
            mLogoWithName.setVisibility(View.VISIBLE);
            blankSpaceWidth = maxTotalWidth - totalWidthWithoutLogo - logoWithNameWidth;
        } else if (totalWidthWithoutLogo + logoWidth <= maxTotalWidth) {
            mLogo.setVisibility(View.VISIBLE);
            mLogoWithName.setVisibility(View.GONE);
            blankSpaceWidth = maxTotalWidth - totalWidthWithoutLogo - logoWidth;
        } else {
            mLogo.setVisibility(View.GONE);
            mLogoWithName.setVisibility(View.GONE);
            blankSpaceWidth =
                    maxTotalWidth < totalWidthWithoutLogo
                            ? 0
                            : maxTotalWidth - totalWidthWithoutLogo;
            assert maxTotalWidth >= totalWidthWithoutLogo
                    : "Screen width is expected to fit the two buttons at least.";
        }

        // Sets the blank space width.
        mSpace.getLayoutParams().width = blankSpaceWidth;
        measureChild(
                mSpace,
                MeasureSpec.makeMeasureSpec(blankSpaceWidth, MeasureSpec.EXACTLY),
                heightMeasureSpec);

        // Note that mLogoWithName and mLogo must have the same height.
        int maxMeasuredHeight =
                Math.max(
                        mLogoWithName.getMeasuredHeight(),
                        Math.max(
                                mPrimaryButton.getMeasuredHeight(),
                                mSecondaryButton.getMeasuredHeight()));
        int totalHeightWithPadding =
                maxMeasuredHeight
                        + getPaddingTop()
                        + getPaddingBottom()
                        + layoutParams.topMargin
                        + layoutParams.bottomMargin;
        int measuredHeightSpec =
                MeasureSpec.makeMeasureSpec(totalHeightWithPadding, MeasureSpec.EXACTLY);
        setMeasuredDimension(widthMeasureSpec, measuredHeightSpec);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) getLayoutParams();
        int leftSpace = getPaddingLeft() + layoutParams.leftMargin;
        int rightSpace = getPaddingRight() + layoutParams.rightMargin;
        int topSpace = getPaddingTop() + layoutParams.topMargin;
        int bottomSpace = getPaddingBottom() + layoutParams.bottomMargin;
        int viewHeight = bottom - top;
        int viewWidth = right - left;
        int childVerticalSpace = viewHeight - topSpace - bottomSpace;

        int childLeft;
        int childRight;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if (isRtl) {
            childRight = viewWidth - rightSpace;
            childLeft = childRight;
        } else {
            childLeft = leftSpace;
            childRight = childLeft;
        }

        for (int childIndex = 0; childIndex < getChildCount(); childIndex++) {
            View child = getChildAt(childIndex);
            if (child.getVisibility() == View.GONE) continue;

            int childWidth = child.getMeasuredWidth();
            int childHeight = child.getMeasuredHeight();
            if (isRtl) {
                childRight = childLeft;
                childLeft = childRight - childWidth;
            } else {
                childLeft = childRight;
                childRight = childLeft + childWidth;
            }

            // Center child vertically.
            int childTop = topSpace + (childVerticalSpace - childHeight) / 2;
            child.layout(childLeft, childTop, childRight, childTop + childHeight);
        }
    }
}
