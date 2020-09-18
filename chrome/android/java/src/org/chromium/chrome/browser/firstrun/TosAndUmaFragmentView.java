// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.chrome.R;

/**
 * Base view for fre_tosanduma.xml. This view may change child view placement when changing screen
 * dimensions (e.g. on rotation).
 */
public class TosAndUmaFragmentView extends FrameLayout {
    private ScrollView mScrollView;

    private LinearLayout mMainLayout;

    // The "title and content" contains the mTitle, mContentWrapper, and mLoadingSpinner that is
    // visible when waiting for policy to be loaded.
    private LinearLayout mTitleAndContent;

    // The "content wrapper" contains the ToS text and the UMA check box.
    private LinearLayout mContentWrapper;

    // The "bottom group" contains the accept & continue button, and a small spinner that displays
    // in its place when waiting for C++ to load before processing the FRE screen.
    private FrameLayout mBottomGroup;

    private View mTitle;
    private View mLogo;
    private View mLoadingSpinnerContainer;
    private View mShadow;

    private int mLastHeight;
    private int mLastWidth;

    // Spacing params
    private int mImageBottomMargin;
    private int mVerticalSpacing;
    private int mImageSize;
    private int mLoadingSpinnerSize;
    private int mLandscapeTopPadding;
    private int mHeadlineSize;
    private int mContentMargin;
    private int mButtonBarHeight;

    /**
     * Constructor for inflating via XML.
     */
    public TosAndUmaFragmentView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.scroll_view);

        mMainLayout = findViewById(R.id.fre_main_layout);
        mTitleAndContent = findViewById(R.id.fre_title_and_content);
        mContentWrapper = findViewById(R.id.fre_content_wrapper);
        mBottomGroup = findViewById(R.id.fre_bottom_group);

        mTitle = findViewById(R.id.title);
        mLogo = findViewById(R.id.image);
        mLoadingSpinnerContainer = findViewById(R.id.loading_view_container);
        mShadow = findViewById(R.id.shadow);

        // Set up shadow.
        // Needed when scrolling to/away from the bottom of the ScrollView.
        mScrollView.getViewTreeObserver().addOnScrollChangedListener(this::updateShadowVisibility);
        // Needed when other elements are added / removed from ScrollView.
        mScrollView.getViewTreeObserver().addOnGlobalLayoutListener(this::updateShadowVisibility);

        // Cache resource dimensions that used in #onMeasure.
        mImageBottomMargin = getResources().getDimensionPixelSize(R.dimen.fre_image_bottom_margin);
        mVerticalSpacing = getResources().getDimensionPixelSize(R.dimen.fre_vertical_spacing);
        mImageSize = getResources().getDimensionPixelSize(R.dimen.fre_tos_image_height);
        mLoadingSpinnerSize =
                getResources().getDimensionPixelSize(R.dimen.fre_loading_spinner_size);
        mLandscapeTopPadding =
                getResources().getDimensionPixelSize(R.dimen.fre_landscape_top_padding);
        mHeadlineSize = getResources().getDimensionPixelSize(R.dimen.headline_size);
        mContentMargin = getResources().getDimensionPixelSize(R.dimen.fre_content_margin);
        mButtonBarHeight = getResources().getDimensionPixelSize(R.dimen.fre_tos_button_bar_height);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // For an alternate layout in horizontal mode for screens of a certain size. These are why
        // the padding is set manually.

        // This assumes that view's layout_width is set to match_parent.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.EXACTLY;

        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);

        // If the layout orientation does not change, there's no need to recalculate layout
        // attributes.
        if (width != mLastWidth || height != mLastHeight) {
            mLastHeight = height;
            mLastWidth = width;

            boolean useWideScreenLayout = shouldUseWideScreen(width, height);

            mMainLayout.setOrientation(
                    useWideScreenLayout ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);

            mTitleAndContent.setPaddingRelative(mTitleAndContent.getPaddingStart(),
                    getTitleAndContentLayoutTopPadding(useWideScreenLayout),
                    mTitleAndContent.getPaddingEnd(), mTitleAndContent.getPaddingBottom());

            setLogoLayoutParams(useWideScreenLayout, height);
            setTitleLayoutParams(useWideScreenLayout);
            setSpinnerLayoutParams(useWideScreenLayout, width, height);

            setContentLayoutParams(useWideScreenLayout);

            setBottomGroupLayoutParams(useWideScreenLayout);
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        updateShadowVisibility();
    }

    private boolean shouldUseWideScreen(int width, int height) {
        return (height >= mImageSize + 2 * mButtonBarHeight) && (width > 1.5 * height);
    }

    private void updateShadowVisibility() {
        if (mScrollView.canScrollVertically(1)) {
            mShadow.setVisibility(VISIBLE);
            mShadow.bringToFront();
        } else {
            mShadow.setVisibility(GONE);
        }
    }

    private void setSpinnerLayoutParams(boolean useWideScreen, int width, int height) {
        LinearLayout.LayoutParams spinnerParams =
                (LinearLayout.LayoutParams) mLoadingSpinnerContainer.getLayoutParams();

        // Adjust the spinner placement. If in portrait mode, the spinner is placed in the region
        // below the title; If in wide screen mode, the spinner is placed in the center of
        // the entire screen. Because we cannot get the exact size for headline,
        // the spinner placement is approximately centered in this case.
        if (useWideScreen) {
            int freImageWidth = mImageSize + mVerticalSpacing * 2;
            int spinnerStartMargin =
                    Math.max(0, (width / 2) - freImageWidth - mLoadingSpinnerSize / 2);

            int topContentHeight = mHeadlineSize + mLandscapeTopPadding;
            int spinnerTopMargin =
                    Math.max(0, height / 2 - topContentHeight - mLoadingSpinnerSize / 2);

            spinnerParams.gravity = Gravity.START;
            spinnerParams.setMarginStart(spinnerStartMargin);
            spinnerParams.topMargin = spinnerTopMargin;
        } else {
            // Use the same padding between title and logo for the spinner.
            // TODO(crbug.com/1128123): Switch from top margin to an approach that will center the
            //  spinner in the bottom half of the screen.
            int spinnerTopMargin = mImageBottomMargin;

            spinnerParams.gravity = Gravity.CENTER_HORIZONTAL;
            spinnerParams.setMarginStart(0);
            spinnerParams.topMargin = spinnerTopMargin;
        }

        mLoadingSpinnerContainer.setLayoutParams(spinnerParams);
    }

    private void setLogoLayoutParams(boolean useWideScreen, int height) {
        LinearLayout.LayoutParams logoLayoutParams =
                (LinearLayout.LayoutParams) mLogo.getLayoutParams();
        if (useWideScreen) {
            // When using the wide screen layout, we want to vertically center the logo on the start
            // side of the screen. While we have no padding on the main layout when using the wide
            // screen, we'll calculate the space needed and set it as top margin above the logo to
            // make it centered.
            int topMargin = (height - mImageSize) / 2;

            // We only use the wide screen layout when the screen height is tall enough to
            // accommodate the image and some padding. But just in case that calculation fails,
            // ensure topMargin isn't negative.
            assert topMargin > 0;
            logoLayoutParams.topMargin = Math.max(0, topMargin);
            logoLayoutParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.TOP;
        } else {
            // Otherwise, in tall screen mode, we want the align the baseline of the title to the
            // center of the screen. While calculation is done in a similar way, we are putting
            // mVerticalSpacing for marginTop as minimum to avoid 0dp spacing between top and logo
            // on small screen devices.
            int freImageHeight = mImageSize + mImageBottomMargin;
            logoLayoutParams.topMargin =
                    Math.max(mVerticalSpacing, (height / 2 - freImageHeight - mHeadlineSize));
            logoLayoutParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
        }
    }

    private void setTitleLayoutParams(boolean useWideScreen) {
        LinearLayout.LayoutParams titleParams =
                (LinearLayout.LayoutParams) mTitle.getLayoutParams();
        titleParams.gravity = useWideScreen ? Gravity.START : Gravity.CENTER;
    }

    private void setContentLayoutParams(boolean useWideScreen) {
        MarginLayoutParams contentWrapperLayoutParams =
                (MarginLayoutParams) mContentWrapper.getLayoutParams();
        contentWrapperLayoutParams.setMarginStart(useWideScreen ? 0 : mContentMargin);
        mContentWrapper.setLayoutParams(contentWrapperLayoutParams);
    }

    private void setBottomGroupLayoutParams(boolean useWideScreen) {
        FrameLayout.LayoutParams bottomGroupParams =
                (FrameLayout.LayoutParams) mBottomGroup.getLayoutParams();
        bottomGroupParams.gravity = useWideScreen ? Gravity.END | Gravity.BOTTOM
                                                  : Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
    }

    private int getTitleAndContentLayoutTopPadding(boolean useWideScreen) {
        return useWideScreen ? mLandscapeTopPadding : 0;
    }
}
