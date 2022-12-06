// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;

import org.chromium.chrome.R;

/**
 * Base view for fre_tosanduma.xml. This view may change child view placement when changing screen
 * dimensions (e.g. on rotation).
 *
 * See https://crbug.com/1151537 for illustration.
 */
public class TosAndUmaFragmentView extends RelativeLayout {
    private ScrollView mScrollView;

    private LinearLayout mMainLayout;

    // The "title and content" contains the mTitle, mContentWrapper, and mLoadingSpinner that is
    // visible when waiting for policy to be loaded.
    private View mTitleAndContent;

    // The "content wrapper" contains the ToS text and the UMA check box.
    private View mContentWrapper;

    // The "bottom group" contains the accept & continue button, and a small spinner that displays
    // in its place when waiting for C++ to load before processing the FRE screen.
    private View mBottomGroup;

    private View mTitle;
    private View mLogo;
    private View mLoadingSpinnerContainer;
    private View mPrivacyDisclaimer;
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
    private int mAcceptButtonHeight;
    private int mBottomGroupVerticalMarginRegular;
    private int mBottomGroupVerticalMarginSmall;

    // Store the bottom margins for different screen orientations. We are using a smaller bottom
    // margin when the content becomes scrollable. Storing margins per orientation because there are
    // cases where content is scrollable in landscape mode while not in portrait mode.
    private int mBottomMarginPortrait;
    private int mBottomMarginLandscape;

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
        mPrivacyDisclaimer = findViewById(R.id.privacy_disclaimer);
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
        mAcceptButtonHeight = getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);

        mBottomGroupVerticalMarginRegular =
                getResources().getDimensionPixelSize(R.dimen.fre_button_vertical_margin);
        mBottomGroupVerticalMarginSmall =
                getResources().getDimensionPixelSize(R.dimen.fre_button_vertical_margin_small);

        // Default bottom margin to "regular", consistent with what is defined in xml.
        mBottomMarginPortrait = mBottomGroupVerticalMarginRegular;
        mBottomMarginLandscape = mBottomGroupVerticalMarginRegular;
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
            setPrivacyDisclaimerLayoutParams(useWideScreenLayout);

            setBottomGroupLayoutParams(useWideScreenLayout);
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Do another round of view adjustments that depends on sizes assigned to children views in
        // super#onMeasure. If the state of any view is changed in this process, trigger another
        // round of measure to make changes take effect.
        boolean changed = doPostMeasureAdjustment();
        if (changed) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    private boolean shouldUseWideScreen(int width, int height) {
        int maxButtonBarHeight = mAcceptButtonHeight + 2 * mBottomGroupVerticalMarginRegular;
        return (height >= mImageSize + 2 * maxButtonBarHeight) && (width > 1.5 * height);
    }

    /**
     * Adjust views after measure, when every view components has an initial size assigned.
     * @return Whether any change happened to children views.
     */
    private boolean doPostMeasureAdjustment() {
        boolean changed = updateShadowVisibility();
        changed |= assignSmallBottomMarginIfNecessary();
        return changed;
    }

    private boolean updateShadowVisibility() {
        int newVisibility = mScrollView.canScrollVertically(1) ? VISIBLE : GONE;
        if (newVisibility == mShadow.getVisibility()) {
            return false;
        }
        mShadow.setVisibility(newVisibility);
        return true;
    }

    /**
     * When content is scrollable, use a smaller margin to present more content on screen.
     * Note that once we change to using a smaller margin we currently will never switch back to the
     * default margin size (e.g. enter then exit multi-window).
     *
     * TODO(https://crbug.com/1159198): Adjust the margin according to the size of
     * TosAndUmaFragmentView.
     */
    private boolean assignSmallBottomMarginIfNecessary() {
        // Check the width and height of TosAndUmaFragmentView. This function may be executed
        // between transitioning from landscape to portrait. If the current measure spec (mLastWidth
        // and mLastHeight) is different than the size actually measured (getHeight() &&
        // getWidth()), the results from mScrollView#canScrollVertically could be stale. In such
        // cases, it is safe to early return here, as current measure is in transition and a
        // follow-up measure will be triggered when the measured spec and actual size matches.
        if (getHeight() != mLastHeight || getWidth() != mLastWidth) {
            return false;
        }

        // Do not assign margins if the content is not scrollable.
        if (!mScrollView.canScrollVertically(1) && !mScrollView.canScrollVertically(-1)) {
            return false;
        }

        MarginLayoutParams params = (MarginLayoutParams) mBottomGroup.getLayoutParams();
        if (params.bottomMargin == mBottomGroupVerticalMarginSmall) {
            return false;
        }

        if (shouldUseLandscapeBottomMargin()) {
            mBottomMarginLandscape = mBottomGroupVerticalMarginSmall;
        } else {
            mBottomMarginPortrait = mBottomGroupVerticalMarginSmall;
        }
        params.setMargins(params.leftMargin, mBottomGroupVerticalMarginSmall, params.rightMargin,
                mBottomGroupVerticalMarginSmall);
        mBottomGroup.setLayoutParams(params);
        return true;
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

    private void setPrivacyDisclaimerLayoutParams(boolean useWideScreen) {
        LinearLayout.LayoutParams privacyDisclaimerParams =
                (LinearLayout.LayoutParams) mPrivacyDisclaimer.getLayoutParams();
        privacyDisclaimerParams.gravity = useWideScreen ? Gravity.START : Gravity.CENTER;
        privacyDisclaimerParams.setMarginStart(useWideScreen ? 0 : mContentMargin);
        mPrivacyDisclaimer.setLayoutParams(privacyDisclaimerParams);
    }

    private void setBottomGroupLayoutParams(boolean useWideScreen) {
        RelativeLayout.LayoutParams bottomGroupParams =
                (RelativeLayout.LayoutParams) mBottomGroup.getLayoutParams();
        int removedRule =
                useWideScreen ? RelativeLayout.CENTER_HORIZONTAL : RelativeLayout.ALIGN_PARENT_END;
        int addedRule =
                useWideScreen ? RelativeLayout.ALIGN_PARENT_END : RelativeLayout.CENTER_HORIZONTAL;
        bottomGroupParams.removeRule(removedRule);
        bottomGroupParams.addRule(addedRule);

        int bottomMargin =
                shouldUseLandscapeBottomMargin() ? mBottomMarginLandscape : mBottomMarginPortrait;
        bottomGroupParams.setMargins(bottomGroupParams.leftMargin, bottomMargin,
                bottomGroupParams.rightMargin, bottomMargin);
        mBottomGroup.setLayoutParams(bottomGroupParams);
    }

    private int getTitleAndContentLayoutTopPadding(boolean useWideScreen) {
        return useWideScreen ? mLandscapeTopPadding : 0;
    }

    private boolean shouldUseLandscapeBottomMargin() {
        return mLastWidth > mLastHeight;
    }
}
