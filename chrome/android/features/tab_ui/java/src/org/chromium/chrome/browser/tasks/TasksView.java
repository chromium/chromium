// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoDescriptionView;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.base.WindowAndroid;

// The view of the tasks surface.
class TasksView extends CoordinatorLayoutForPointer {
    private static final int OMNIBOX_BOTTOM_PADDING_DP = 4;

    private final Context mContext;
    private FrameLayout mCarouselTabSwitcherContainer;
    private AppBarLayout mHeaderView;
    private AppBarLayout.OnOffsetChangedListener mFakeSearchBoxShrinkAnimation;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private IncognitoDescriptionView mIncognitoDescriptionView;
    private View.OnClickListener mIncognitoDescriptionLearnMoreListener;
    private boolean mIncognitoCookieControlsCardIsVisible;
    private boolean mIncognitoCookieControlsToggleIsChecked;
    private OnCheckedChangeListener mIncognitoCookieControlsToggleCheckedListener;
    private @CookieControlsEnforcement int mIncognitoCookieControlsToggleEnforcement =
            CookieControlsEnforcement.NO_ENFORCEMENT;
    private View.OnClickListener mIncognitoCookieControlsIconClickListener;
    private UiConfig mUiConfig;
    private boolean mIsIncognito;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito, WindowAndroid windowAndroid) {
        assert mSearchBoxCoordinator
                != null : "#onFinishInflate should be completed before the call to initialize.";

        mSearchBoxCoordinator.initialize(activityLifecycleDispatcher, isIncognito, windowAndroid);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCarouselTabSwitcherContainer =
                (FrameLayout) findViewById(R.id.carousel_tab_switcher_container);
        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);
        mHeaderView = (AppBarLayout) findViewById(R.id.task_surface_header);
        mUiConfig = new UiConfig(this);
        setHeaderPadding();
        setTabCarouselTitleStyle();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mUiConfig.updateDisplayStyle();
        alignHeaderForFeed();
    }

    private void setTabCarouselTitleStyle() {
        // Match the tab carousel title style with the feed header.
        // There are many places checking FeedFeatures.isReportingUserActions, like in
        // ExploreSurfaceCoordinator.
        TextView titleDescription = (TextView) findViewById(R.id.tab_switcher_title_description);
        TextView moreTabs = (TextView) findViewById(R.id.more_tabs);
        ApiCompatibilityUtils.setTextAppearance(
                titleDescription, R.style.TextAppearance_TextAccentMediumThick_Secondary);
        ApiCompatibilityUtils.setTextAppearance(moreTabs, R.style.TextAppearance_Button_Text_Blue);
        ViewCompat.setPaddingRelative(titleDescription, titleDescription.getPaddingStart(),
                titleDescription.getPaddingTop(), titleDescription.getPaddingEnd(),
                titleDescription.getPaddingBottom());
    }

    ViewGroup getCarouselTabSwitcherContainer() {
        return mCarouselTabSwitcherContainer;
    }

    ViewGroup getBodyViewContainer() {
        return findViewById(R.id.tasks_surface_body);
    }

    /**
     * Set the visibility of the tasks surface body.
     * @param isVisible Whether it's visible.
     */
    void setSurfaceBodyVisibility(boolean isVisible) {
        getBodyViewContainer().setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the visibility of the tab carousel.
     * @param isVisible Whether it's visible.
     */
    void setTabCarouselVisibility(boolean isVisible) {
        mCarouselTabSwitcherContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        findViewById(R.id.tab_switcher_title).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return The {@link SearchBoxCoordinator} representing the fake search box.
     */
    SearchBoxCoordinator getSearchBoxCoordinator() {
        return mSearchBoxCoordinator;
    }

    /**
     * Set the visibility of the Most Visited Tiles.
     */
    void setMostVisitedVisibility(int visibility) {
        findViewById(R.id.mv_tiles_container).setVisibility(visibility);
    }

    /**
     * Set the {@link android.view.View.OnClickListener} for More Tabs.
     */
    void setMoreTabsOnClickListener(@Nullable View.OnClickListener listener) {
        findViewById(R.id.more_tabs).setOnClickListener(listener);
    }

    /**
     * Set the incognito state.
     * @param isIncognito Whether it's in incognito mode.
     */
    void setIncognitoMode(boolean isIncognito) {
        Resources resources = mContext.getResources();
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(resources, isIncognito);
        setBackgroundColor(backgroundColor);
        mHeaderView.setBackgroundColor(backgroundColor);

        mSearchBoxCoordinator.setIncognitoMode(isIncognito);
        mSearchBoxCoordinator.setBackground(AppCompatResources.getDrawable(mContext,
                isIncognito ? R.drawable.fake_search_box_bg_incognito : R.drawable.ntp_search_box));
        int hintTextColor = isIncognito
                ? ApiCompatibilityUtils.getColor(resources, R.color.locationbar_light_hint_text)
                : ApiCompatibilityUtils.getColor(resources, R.color.locationbar_dark_hint_text);
        mSearchBoxCoordinator.setSearchBoxHintColor(hintTextColor);
        mIsIncognito = isIncognito;
    }

    /**
     * Initialize incognito description view.
     * Note that this interface is supposed to be called only once.
     */
    void initializeIncognitoDescriptionView() {
        assert mIncognitoDescriptionView == null;
        ViewStub containerStub =
                (ViewStub) findViewById(R.id.incognito_description_container_layout_stub);
        View containerView = containerStub.inflate();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            containerView.setFocusable(true);
            containerView.setFocusableInTouchMode(true);
        }
        mIncognitoDescriptionView = (IncognitoDescriptionView) containerView.findViewById(
                R.id.new_tab_incognito_container);
        if (mIncognitoDescriptionLearnMoreListener != null) {
            setIncognitoDescriptionLearnMoreClickListener(mIncognitoDescriptionLearnMoreListener);
        }
        setIncognitoCookieControlsCardVisibility(mIncognitoCookieControlsCardIsVisible);
        setIncognitoCookieControlsToggleChecked(mIncognitoCookieControlsToggleIsChecked);
        if (mIncognitoCookieControlsToggleCheckedListener != null) {
            setIncognitoCookieControlsToggleCheckedListener(
                    mIncognitoCookieControlsToggleCheckedListener);
        }
        setIncognitoCookieControlsToggleEnforcement(mIncognitoCookieControlsToggleEnforcement);
        if (mIncognitoCookieControlsIconClickListener != null) {
            setIncognitoCookieControlsIconClickListener(mIncognitoCookieControlsIconClickListener);
        }
    }

    /**
     * Set the visibility of the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoDescriptionVisibility(boolean isVisible) {
        mIncognitoDescriptionView.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the incognito description learn more click listener.
     * @param listener The given click listener.
     */
    void setIncognitoDescriptionLearnMoreClickListener(View.OnClickListener listener) {
        mIncognitoDescriptionLearnMoreListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.findViewById(R.id.learn_more).setOnClickListener(listener);
            mIncognitoDescriptionLearnMoreListener = null;
        }
    }

    /**
     * Set the visibility of the cookie controls card on the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoCookieControlsCardVisibility(boolean isVisible) {
        mIncognitoCookieControlsCardIsVisible = isVisible;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.showCookieControlsCard(isVisible);
        }
    }

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mIncognitoCookieControlsToggleIsChecked = isChecked;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggle(isChecked);
        }
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mIncognitoCookieControlsToggleCheckedListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
            mIncognitoCookieControlsToggleCheckedListener = null;
        }
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mIncognitoCookieControlsToggleEnforcement = enforcement;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsEnforcement(enforcement);
        }
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconClickListener(OnClickListener listener) {
        mIncognitoCookieControlsIconClickListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsIconOnclickListener(listener);
            mIncognitoCookieControlsIconClickListener = null;
        }
    }

    /**
     * Set the top margin for the tasks surface body.
     * @param topMargin The top margin to set.
     */
    void setTasksSurfaceBodyTopMargin(int topMargin) {
        MarginLayoutParams params = (MarginLayoutParams) getBodyViewContainer().getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Set the top margin for the mv tiles container.
     * @param topMargin The top margin to set.
     */
    void setMVTilesContainerTopMargin(int topMargin) {
        MarginLayoutParams params =
                (MarginLayoutParams) mHeaderView.findViewById(R.id.mv_tiles_container)
                        .getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Set the top margin for the tab switcher title.
     * @param topMargin The top margin to set.
     */
    void setTabSwitcherTitleTopMargin(int topMargin) {
        MarginLayoutParams params =
                (MarginLayoutParams) mHeaderView.findViewById(R.id.tab_switcher_title)
                        .getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Reset the scrolling position by expanding the {@link #mHeaderView}.
     */
    void resetScrollPosition() {
        if (mHeaderView != null && mHeaderView.getHeight() != mHeaderView.getBottom()) {
            mHeaderView.setExpanded(true);
        }
    }
    /**
     * Add a header offset change listener.
     * @param onOffsetChangedListener The given header offset change listener.
     */
    void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.addOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Remove the given header offset change listener.
     * @param onOffsetChangedListener The header offset change listener which should be removed.
     */
    void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.removeOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Create the fake box shrink animation if it doesn't exist yet and add the omnibox shrink
     * animation when the homepage is scrolled.
     */
    void addFakeSearchBoxShrinkAnimation() {
        if (mHeaderView == null) return;
        if (mFakeSearchBoxShrinkAnimation == null) {
            int fakeSearchBoxHeight =
                    getResources().getDimensionPixelSize(R.dimen.ntp_search_box_height);
            int toolbarContainerTopMargin =
                    getResources().getDimensionPixelSize(R.dimen.location_bar_vertical_margin);
            View fakeSearchBoxView = findViewById(R.id.search_box);
            View searchTextView = findViewById(R.id.search_box_text);
            if (fakeSearchBoxView == null) return;
            // If fake search box view is not null when creating this animation, it will not change.
            // So checking it once above is enough.
            mFakeSearchBoxShrinkAnimation = (appbarLayout, headerVerticalOffset)
                    -> updateFakeSearchBoxShrinkAnimation(headerVerticalOffset, fakeSearchBoxHeight,
                            toolbarContainerTopMargin, fakeSearchBoxView, searchTextView);
        }
        mHeaderView.addOnOffsetChangedListener(mFakeSearchBoxShrinkAnimation);
    }

    /** Remove the fake box shrink animation. */
    void removeFakeSearchBoxShrinkAnimation() {
        if (mHeaderView != null) {
            mHeaderView.removeOnOffsetChangedListener(mFakeSearchBoxShrinkAnimation);
        }
    }

    /**
     * When the start surface toolbar is about to be scrolled out of the screen and the fake search
     * box is almost at the screen top, start to reduce its height to make it finally the same as
     * toolbar container view's height. This makes fake search box exactly overlap the toolbar
     * container view and makes the transition smooth.
     *
     * <p>This function should be called together with
     * StartSurfaceToolbarMediator#updateTranslationY, which scroll up the start surface toolbar
     * together with the header.
     *
     * @param headerOffset The current offset of the header.
     * @param originalFakeSearchBoxHeight The height of fake search box.
     * @param toolbarContainerTopMargin The top margin of toolbar container view.
     * @param fakeSearchBox The fake search box in start surface homepage.
     * @param searchTextView  The search text view in fake search box.
     */
    private void updateFakeSearchBoxShrinkAnimation(int headerOffset,
            int originalFakeSearchBoxHeight, int toolbarContainerTopMargin, View fakeSearchBox,
            View searchTextView) {
        // When the header is scrolled up by fake search box height or so, reduce the fake search
        // box height.
        int reduceHeight = MathUtils.clamp(
                -headerOffset - originalFakeSearchBoxHeight, 0, toolbarContainerTopMargin);

        ViewGroup.LayoutParams layoutParams = fakeSearchBox.getLayoutParams();
        if (layoutParams.height == originalFakeSearchBoxHeight - reduceHeight) {
            return;
        }

        layoutParams.height = originalFakeSearchBoxHeight - reduceHeight;

        // Update the top margin of the fake search box.
        ViewGroup.MarginLayoutParams marginLayoutParams =
                (ViewGroup.MarginLayoutParams) fakeSearchBox.getLayoutParams();
        marginLayoutParams.setMargins(marginLayoutParams.leftMargin, reduceHeight,
                marginLayoutParams.rightMargin, marginLayoutParams.bottomMargin);

        fakeSearchBox.setLayoutParams(layoutParams);

        // Update the translation X of search text view to make space for the search logo.
        SearchEngineLogoUtils searchEngineLogoUtils = SearchEngineLogoUtils.getInstance();
        assert searchEngineLogoUtils != null;

        if (!searchEngineLogoUtils.shouldShowSearchEngineLogo(mIsIncognito)) {
            return;
        }

        int finalTranslationX =
                getResources().getDimensionPixelSize(R.dimen.location_bar_icon_end_padding_focused)
                - getResources().getDimensionPixelSize(R.dimen.location_bar_icon_end_padding);
        searchTextView.setTranslationX(
                finalTranslationX * ((float) reduceHeight / toolbarContainerTopMargin));
    }

    /**
     * Make the padding of header consistent with that of Feed recyclerview which is sized by {@link
     * ViewResizer} in {@link FeedSurfaceCoordinator}
     */
    private void setHeaderPadding() {
        int defaultPadding = 0;
        int widePadding =
                getResources().getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        ViewResizer.createAndAttach(mHeaderView, mUiConfig, defaultPadding, widePadding);
        alignHeaderForFeed();
    }

    /**
     * Feed has extra content padding, we need to align the header with it. However, the padding
     * of the header is already bound with ViewResizer in setHeaderPadding(), so we update the left
     * & right margins of MV tiles container and carousel tab switcher container.
     */
    private void alignHeaderForFeed() {
        MarginLayoutParams mostVisitedLayoutParams =
                (MarginLayoutParams) mHeaderView.findViewById(R.id.mv_tiles_container)
                        .getLayoutParams();

        MarginLayoutParams carouselTabSwitcherParams =
                (MarginLayoutParams) mCarouselTabSwitcherContainer.getLayoutParams();
            mostVisitedLayoutParams.leftMargin = 0;
            mostVisitedLayoutParams.rightMargin = 0;
            carouselTabSwitcherParams.leftMargin = 0;
            carouselTabSwitcherParams.rightMargin = 0;
    }
}
