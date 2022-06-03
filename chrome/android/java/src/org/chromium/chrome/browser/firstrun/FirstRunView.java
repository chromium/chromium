// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;

/**
 * A page shown during the First Run Experience. It modifies the layout for a better appearance
 * for certain screen dimensions.
 */
public class FirstRunView extends FrameLayout {
    private static final String TAG = "FirstRunView";

    private View mMainLayout;
    private LinearLayout mImageAndContent;
    private LinearLayout mContentWrapper;

    /**
     * Constructor for inflating via XML.
     */
    public FirstRunView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // TODO(peconn): Unify the common parts of the FirstRunView layouts.
        // TODO(peconn): Use different layout files for the landscape and horizontal parts

        mMainLayout = findViewById(R.id.fre_main_layout);
        mImageAndContent = (LinearLayout) findViewById(R.id.fre_image_and_content);
        mContentWrapper = (LinearLayout) findViewById(R.id.fre_content_wrapper);


    }

    protected boolean isHorizontalModeEnabled() {
        return true;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // There was a requirement to have the titles and images of all of the first run experience
        // pages to be vertically aligned so the transitions between pages look nice.
        // The other requirement is for an alternate layout in horizontal mode for screens of a
        // certain size. These are why the padding is set manually.

        // This assumes that view's layout_width is set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;

        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        MarginLayoutParams contentWrapperLayout =
                (MarginLayoutParams) mContentWrapper.getLayoutParams();

        int imageAndContentPaddingStart = 0;
        int halfContentHeight = 0;
        // The AccountSigninView is part of the First Run Experience, and it's image carousel is
        // the widest of the 'image's to be displayed in the FRE.
        if (isHorizontalModeEnabled()
                && width >= 2 * getResources().getDimension(R.dimen.signin_image_carousel_width)
                && width > height) {
            mImageAndContent.setOrientation(LinearLayout.HORIZONTAL);
            imageAndContentPaddingStart = getResources().getDimensionPixelSize(R.dimen.fre_margin);

            contentWrapperLayout.width = 0;
            contentWrapperLayout.height = LayoutParams.WRAP_CONTENT;
            contentWrapperLayout.topMargin = 0;

            halfContentHeight = getResources().getDimensionPixelSize(R.dimen.headline_size)
                    + getResources().getDimensionPixelSize(R.dimen.fre_vertical_spacing)
                    + getResources().getDimensionPixelSize(R.dimen.fre_image_height) / 2;

        } else {
            mImageAndContent.setOrientation(LinearLayout.VERTICAL);

            contentWrapperLayout.width = LayoutParams.WRAP_CONTENT;
            contentWrapperLayout.height = 0;
            contentWrapperLayout.topMargin =
                    getResources().getDimensionPixelSize(R.dimen.fre_vertical_spacing);

            halfContentHeight = getResources().getDimensionPixelSize(R.dimen.headline_size)
                    + getResources().getDimensionPixelSize(R.dimen.fre_vertical_spacing)
                    + getResources().getDimensionPixelSize(R.dimen.fre_image_height)
                    + getResources().getDimensionPixelSize(R.dimen.fre_vertical_spacing);
        }

        // Add padding to get it roughly centered.
        int topPadding = Math.max(0, (height / 2) - halfContentHeight);

        mMainLayout.setPadding(mMainLayout.getPaddingLeft(), topPadding,
                mMainLayout.getPaddingRight(), mMainLayout.getPaddingBottom());

        ViewCompat.setPaddingRelative(mImageAndContent, imageAndContentPaddingStart,
                mImageAndContent.getPaddingTop(), ViewCompat.getPaddingEnd(mImageAndContent),
                mImageAndContent.getPaddingBottom());

        mContentWrapper.setLayoutParams(contentWrapperLayout);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

}

