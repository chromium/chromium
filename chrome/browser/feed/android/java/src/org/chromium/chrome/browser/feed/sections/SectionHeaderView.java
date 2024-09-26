// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.FeedUma;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * View for the header of the personalized feed that has a context menu to manage the feed.
 *
 * <p>This view can be inflated from one of two layouts, hence many @Nullables.
 */
public class SectionHeaderView extends LinearLayout {
    /** OnTabSelectedListener that delegates calls to the SectionHeadSelectedListener. */
    private static class SectionHeaderTabListener implements TabLayout.OnTabSelectedListener {
        private @Nullable OnSectionHeaderSelectedListener mListener;

        @Override
        public void onTabSelected(TabLayout.Tab tab) {
            if (mListener != null) {
                mListener.onSectionHeaderSelected(tab.getPosition());
            }
        }

        @Override
        public void onTabUnselected(TabLayout.Tab tab) {
            if (mListener != null) {
                mListener.onSectionHeaderUnselected(tab.getPosition());
            }
        }

        @Override
        public void onTabReselected(TabLayout.Tab tab) {
            if (mListener != null) {
                mListener.onSectionHeaderReselected(tab.getPosition());
            }
        }
    }

    private class UnreadIndicator implements ViewTreeObserver.OnGlobalLayoutListener {
        private View mAnchor;
        private SectionHeaderBadgeDrawable mNewBadge;

        UnreadIndicator(View anchor) {
            mAnchor = anchor;
            mNewBadge = new SectionHeaderBadgeDrawable(SectionHeaderView.this.getContext());
            mAnchor.getViewTreeObserver().addOnGlobalLayoutListener(this);
        }

        void destroy() {
            mAnchor.getViewTreeObserver().removeOnGlobalLayoutListener(this);
            mNewBadge.detach(mAnchor);
        }

        @Override
        public void onGlobalLayout() {
            // attachBadgeDrawable() will crash if mAnchor has no parent. This can happen after the
            // views are detached from the window.
            if (mAnchor.getParent() != null) {
                mNewBadge.attach(mAnchor);
            }
        }
    }

    /** Holds additional state for a tab. */
    private static class TabState {
        // Whether the tab has unread content.
        public boolean hasUnreadContent;
        // Null when unread indicator isn't shown.
        @Nullable public UnreadIndicator unreadIndicator;
        // The text to show on the unreadIndicator, if any.
        public String unreadIndicatorText;
        // The tab's displayed text.
        public String text = "";
        public boolean shouldAnimateIndicator;
    }

    // Views in the header layout that are set during inflate.
    private @Nullable ImageView mLeadingStatusIndicator;
    private @Nullable TabLayout mTabLayout;
    private TextView mTitleView;
    private ListMenuButton mMenuView;

    private @Nullable SectionHeaderTabListener mTabListener;
    private ViewGroup mContent;
    private @Nullable View mOptionsPanel;

    private boolean mTextsEnabled;
    private @Px int mToolbarHeight;
    private @Px int mTouchSize;
    private boolean mIsTablet;

    public SectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mTouchSize = getResources().getDimensionPixelSize(R.dimen.feed_v2_header_menu_touch_size);
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
    }

    public void setToolbarHeight(@Px int toolbarHeight) {
        mToolbarHeight = toolbarHeight;
    }

    public void updateDrawable(int index, boolean isVisible) {
        if (mTabLayout == null || mTabLayout.getTabCount() <= index) return;

        TabLayout.Tab tab = mTabLayout.getTabAt(index);

        ImageView optionsIndicatorView = tab.view.findViewById(R.id.options_indicator);
        // Skip setting visibility if indicator isn't visible.
        if (optionsIndicatorView == null || optionsIndicatorView.getVisibility() != View.VISIBLE) {
            return;
        }

        int actionTitleId;

        if (isVisible) {
            optionsIndicatorView.setImageDrawable(
                    ResourcesCompat.getDrawable(
                            getResources(),
                            R.drawable.mtrl_ic_arrow_drop_up,
                            getContext().getTheme()));
            actionTitleId = R.string.feed_options_dropdown_description_close;
        } else {
            optionsIndicatorView.setImageDrawable(
                    ResourcesCompat.getDrawable(
                            getResources(),
                            R.drawable.mtrl_ic_arrow_drop_down,
                            getContext().getTheme()));
            actionTitleId = R.string.feed_options_dropdown_description;
        }

        tab.view.setOnLongClickListener(
                v -> {
                    mTabListener.onTabReselected(tab);
                    return true;
                });

        ViewCompat.replaceAccessibilityAction(
                tab.view,
                AccessibilityActionCompat.ACTION_LONG_CLICK,
                getResources().getString(actionTitleId),
                (view, arguments) -> {
                    mTabListener.onTabReselected(tab);
                    return true;
                });
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(R.id.header_title);
        mMenuView = findViewById(R.id.header_menu);
        mLeadingStatusIndicator = findViewById(R.id.section_status_indicator);
        mTabLayout = findViewById(R.id.tab_list_view);
        mContent = findViewById(R.id.main_content);

        if (mTabLayout != null) {
            mTabListener = new SectionHeaderTabListener();
            mTabLayout.addOnTabSelectedListener(mTabListener);
            if (mIsTablet) {
                // Sets the default width for the header.
                updateTabLayoutHeaderWidth(false);
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
                mTabLayout.setBackgroundResource(R.drawable.header_title_section_tab_background);
            }
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CONTAINMENT)) {
            MarginLayoutParams contentMarginLayoutParams =
                    (MarginLayoutParams) mContent.getLayoutParams();
            contentMarginLayoutParams.topMargin =
                    getResources()
                            .getDimensionPixelSize(R.dimen.feed_containment_feed_header_top_margin);
        }

        if (mLeadingStatusIndicator != null) {
            MarginLayoutParams indicatorViewMarginLayoutParams =
                    (MarginLayoutParams) mLeadingStatusIndicator.getLayoutParams();
            int tabLayoutLateralMargin =
                    getResources()
                            .getDimensionPixelSize(R.dimen.feed_header_tab_layout_lateral_margin);
            indicatorViewMarginLayoutParams.setMarginEnd(
                    indicatorViewMarginLayoutParams.getMarginEnd() + tabLayoutLateralMargin);
        }

        // #getHitRect() will not be valid until the first layout pass completes. Additionally, if
        // the header's enabled state changes, |mMenuView| will move slightly sideways, and the
        // touch target needs to be adjusted. This is a bit chatty during animations, but it should
        // also be fairly cheap.
        mMenuView.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> adjustTouchDelegate(mMenuView));

        // Ensures that the whole header doesn't get focused for a11y.
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    /** Updates header text for this view. */
    void setHeaderText(String text) {
        mTitleView.setText(text);
    }

    /** Adds a blank tab. */
    void addTab() {
        if (mTabLayout == null) {
            return;
        }

        TabLayout.Tab tab = mTabLayout.newTab();
        tab.setCustomView(R.layout.new_tab_page_section_tab);
        tab.setTag(new TabState());
        mTabLayout.addTab(tab);
        tab.view.setClipToPadding(false);
        tab.view.setClipChildren(false);
        tab.view.setForeground(
                ResourcesCompat.getDrawable(
                        getResources(),
                        R.drawable.header_title_tab_selected_ripple,
                        getContext().getTheme()));

        tab.view.setBackground(
                ResourcesCompat.getDrawable(
                        getResources(),
                        R.drawable.header_title_tab_selected_background,
                        getContext().getTheme()));
        if (mTabLayout.getTabCount() > 1) {
            int startLateralPadding =
                    getResources()
                            .getDimensionPixelSize(R.dimen.multi_feed_header_menu_start_margin);
            mContent.setPadding(startLateralPadding, 0, 0, 0);
        }
    }

    /** Removes a tab. */
    void removeTabAt(int index) {
        if (mTabLayout != null) {
            mTabLayout.removeTabAt(index);
        }
    }

    /** Removes all tabs. */
    void removeAllTabs() {
        if (mTabLayout != null) {
            mTabLayout.removeAllTabs();
        }
    }

    /**
     * Set the properties for the header tab at a particular index to text.
     *
     * Does nothing if index is invalid. Make sure to call addTab() beforehand.
     * @param text Text to set the tab to.
     * @param hasUnreadContent Whether there is unread content.
     * @param unreadContentText Text to put in the unread content badge.
     * @param shouldAnimate whether we should animate the unread content transition.
     * @param index Index of the tab to set.
     */
    void setHeaderAt(
            String text,
            boolean hasUnreadContent,
            String unreadContentText,
            boolean shouldAnimate,
            int index) {
        TabLayout.Tab tab = getTabAt(index);
        if (tab == null) {
            return;
        }
        TabState state = getTabState(tab);

        state.text = text;
        state.hasUnreadContent = hasUnreadContent;
        state.unreadIndicatorText = unreadContentText;
        state.shouldAnimateIndicator = shouldAnimate;
        applyTabState(tab);
    }

    /**
     * Sets the visibility of the options indicator on a tab
     *
     * @param index index of the tab to set options indicator on.
     * @param visibility visibility of the options indicator to use.
     */
    void setOptionsIndicatorVisibilityForHeader(int index, ViewVisibility visibility) {
        TabLayout.Tab tab = getTabAt(index);
        if (tab == null) return;

        if (visibility == ViewVisibility.GONE) {
            int leftPadding = tab.view.getPaddingLeft();
            int rightPadding =
                    tab.view.getPaddingRight()
                            + getResources()
                                    .getDimensionPixelOffset(
                                            R.dimen.feed_header_tab_extra_margin_right);
            tab.view.setPadding(leftPadding, 0, rightPadding, 0);
        }
        ImageView image = tab.view.findViewById(R.id.options_indicator);
        image.setVisibility(ViewVisibility.toVisibility(visibility));

        if (visibility == ViewVisibility.VISIBLE) {
            tab.view.setClickable(true);
            // Sets up a11y and ensures indicator is pointing in the right direction.
            updateDrawable(index, false);
        } else {
            tab.view.setOnLongClickListener(null);
            tab.view.setLongClickable(false);
            // If not visible, remove the expand/collapse actions.
            ViewCompat.replaceAccessibilityAction(
                    tab.view, AccessibilityActionCompat.ACTION_LONG_CLICK, null, null);
        }
    }

    @Nullable
    private TabLayout.Tab getTabAt(int index) {
        return mTabLayout != null ? mTabLayout.getTabAt(index) : null;
    }

    /**
     * @param index The index of the tab to set as active.
     *              Does nothing if index is invalid or already selected.
     */
    void setActiveTab(int index) {
        TabLayout.Tab tab = getTabAt(index);
        if (tab != null && mTabLayout.getSelectedTabPosition() != index) {
            mTabLayout.selectTab(tab);
        }
    }

    /** Sets the listener for tab changes. */
    void setTabChangeListener(OnSectionHeaderSelectedListener listener) {
        if (mTabListener != null) {
            mTabListener.mListener = listener;
        }
    }

    /** Sets the delegate for the gear/settings icon. */
    void setMenuDelegate(ModelList listItems, ListMenu.Delegate listMenuDelegate) {
        mMenuView.setOnClickListener(
                (v) -> {
                    displayMenu(listItems, listMenuDelegate);
                });
    }

    /**
     * Sets whether to show tabs or normal title view.
     *
     * Only works if there is both a tab and title view.
     */
    void setTabMode(boolean isTabMode) {
        if (mTabLayout != null) {
            mTitleView.setVisibility(isTabMode ? GONE : VISIBLE);
            mTabLayout.setVisibility(isTabMode ? VISIBLE : GONE);
        }
    }

    /** Sets whether to have logo or a visibility indicator. */
    void setIsLogo(boolean isLogo) {
        if (mLeadingStatusIndicator == null) return;
        if (isLogo) {
            mLeadingStatusIndicator.setImageResource(R.drawable.ic_logo_googleg_24dp);
            // No tint if Google logo.
            ImageViewCompat.setImageTintMode(mLeadingStatusIndicator, PorterDuff.Mode.DST);
        } else {
            mLeadingStatusIndicator.setImageResource(R.drawable.ic_visibility_off_black);
            // Grey tint if visibility indicator.
            ImageViewCompat.setImageTintMode(mLeadingStatusIndicator, PorterDuff.Mode.SRC_IN);
        }
    }

    /** Sets the visibility of the indicator. */
    void setIndicatorVisibility(ViewVisibility visibility) {
        if (mLeadingStatusIndicator != null) {
            mLeadingStatusIndicator.setVisibility(ViewVisibility.toVisibility(visibility));
        }
    }

    void setOptionsPanel(View optionsView) {
        if (mOptionsPanel != null) {
            removeView(mOptionsPanel);
        }
        addView(optionsView);
        mOptionsPanel = optionsView;
    }

    /**
     * This method sets the sticky header options panel.
     * @param optionsView the sticky header options panel view
     */
    void setStickyHeaderOptionsPanel(View optionsView) {}

    /** Sets whether the texts on the tab layout or title view is enabled. */
    void setTextsEnabled(boolean enabled) {
        mTextsEnabled = enabled;
        if (mTabLayout != null) {
            for (int i = 0; i < mTabLayout.getTabCount(); i++) {
                applyTabState(mTabLayout.getTabAt(i));
            }
            mTabLayout.setEnabled(enabled);
        }
        mTitleView.setEnabled(enabled);
    }

    /**
     * If the unread indicator is shown for the header tab at index, then animate the indicator.
     * Otherwise, ignore and rely on the code for setting the unread indicator to animate.
     */
    void startAnimationForHeader(int index) {
        if (mTabLayout != null) {
            TabLayout.Tab tab = getTabAt(index);
            if (tab == null) {
                return;
            }
            TabState state = getTabState(tab);
            // Skip animating if we don't have anything to animate yet.
            // Rely on the unread indicator visibility controls to start the animation.
            if (state.unreadIndicator == null || state.unreadIndicator.mNewBadge == null) {
                return;
            }
            state.unreadIndicator.mNewBadge.startAnimation();
            state.shouldAnimateIndicator = true;
        }
    }

    /** Shows an IPH on the feed header menu button. */
    public void showMenuIph(UserEducationHelper helper) {
        final ViewRectProvider rectProvider =
                new ViewRectProvider(mMenuView) {
                    // ViewTreeObserver.OnPreDrawListener implementation.
                    @Override
                    public boolean onPreDraw() {
                        boolean result = super.onPreDraw();

                        int minRectBottomPosPx = mToolbarHeight + mMenuView.getHeight() / 2;
                        // Notify that the rectangle is hidden to dismiss the popup if the anchor is
                        // positioned too high.
                        if (getRect().bottom < minRectBottomPosPx) {
                            notifyRectHidden();
                        }

                        return result;
                    }
                };
        int yInsetPx =
                getResources().getDimensionPixelOffset(R.dimen.iph_text_bubble_menu_anchor_y_inset);
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setCircleRadius(
                new PulseDrawable.Bounds() {
                    @Override
                    public float getMaxRadiusPx(Rect bounds) {
                        return Math.max(bounds.width(), bounds.height()) / 2.f;
                    }

                    @Override
                    public float getMinRadiusPx(Rect bounds) {
                        return Math.min(bounds.width(), bounds.height()) / 1.5f;
                    }
                });
        helper.requestShowIPH(
                new IPHCommandBuilder(
                                mMenuView.getContext().getResources(),
                                FeatureConstants.FEED_HEADER_MENU_FEATURE,
                                R.string.ntp_feed_menu_iph,
                                R.string.accessibility_ntp_feed_menu_iph)
                        .setAnchorView(mMenuView)
                        .setDismissOnTouch(false)
                        .setInsetRect(new Rect(0, 0, 0, -yInsetPx))
                        .setAutoDismissTimeout(5 * 1000)
                        .setViewRectProvider(rectProvider)
                        // Set clipChildren is important to make sure the bubble does not get
                        // clipped. Set back for better performance during layout.
                        .setOnShowCallback(
                                () -> {
                                    mContent.setClipChildren(false);
                                    mContent.setClipToPadding(false);
                                })
                        .setOnDismissCallback(
                                () -> {
                                    mContent.setClipChildren(true);
                                    mContent.setClipToPadding(true);
                                })
                        .setHighlightParams(params)
                        .build());
    }

    /** Shows an IPH on the web feed tab in the section header. */
    public void showWebFeedAwarenessIph(
            UserEducationHelper helper, int tabIndex, Runnable scroller) {
        // Stop showing before in the view hierarchy, as this will fail/assert.
        // TODO(crbug.com/40914294): Request IPH after parent set or something.
        if (getParent() == null) return;

        helper.requestShowIPH(
                new IPHCommandBuilder(
                                getContext().getResources(),
                                FeatureConstants.WEB_FEED_AWARENESS_FEATURE,
                                R.string.web_feed_awareness,
                                R.string.web_feed_awareness)
                        .setAnchorView(getTabAt(tabIndex).view)
                        .setOnShowCallback(scroller)
                        .build());
    }

    private void adjustTouchDelegate(View view) {
        Rect rect = new Rect();
        view.getHitRect(rect);

        int halfWidthDelta = Math.max((mTouchSize - view.getWidth()) / 2, 0);
        int halfHeightDelta = Math.max((mTouchSize - view.getHeight()) / 2, 0);

        rect.left -= halfWidthDelta;
        rect.right += halfWidthDelta;
        rect.top -= halfHeightDelta;
        rect.bottom += halfHeightDelta;

        setTouchDelegate(new TouchDelegate(rect, view));
    }

    private void displayMenu(ModelList listItems, ListMenu.Delegate listMenuDelegate) {
        FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_FEED_HEADER_MENU);

        if (listItems == null) {
            assert false : "No list items model to display the menu";
            return;
        }

        if (listMenuDelegate == null) {
            assert false : "No list menu delegate for the menu";
            return;
        }

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mMenuView.getContext(), listItems, listMenuDelegate);

        ListMenuButtonDelegate delegate =
                new ListMenuButtonDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return listMenu;
                    }

                    @Override
                    public RectProvider getRectProvider(View listMenuButton) {
                        ViewRectProvider rectProvider = new ViewRectProvider(listMenuButton);
                        rectProvider.setIncludePadding(true);
                        rectProvider.setInsetPx(0, 0, 0, 0);
                        return rectProvider;
                    }
                };

        mMenuView.setDelegate(delegate);
        mMenuView.tryToFitLargestItem(true);
        mMenuView.showMenu();
    }

    private TabState getTabState(TabLayout.Tab tab) {
        return (TabState) tab.getTag();
    }

    /** Updates the view for changes made to TabState or mTextsEnabled. */
    void applyTabState(TabLayout.Tab tab) {
        TabState state = getTabState(tab);
        tab.setText(state.text);
        tab.view.setClickable(mTextsEnabled);
        tab.view.setEnabled(mTextsEnabled);
        adjustTouchDelegate(tab.view);

        // Unread indicator is removed in the updated UI.
        if (FeedFeatures.isFeedFollowUiUpdateEnabled()) {
            return;
        }

        String contentDescription = state.text;
        if (state.hasUnreadContent && mTextsEnabled) {
            contentDescription =
                    contentDescription
                            + ", "
                            + getResources()
                                    .getString(R.string.accessibility_ntp_following_unread_content);

            // The unread indicator is re-created on every update is because the sticky header
            // gets the wrong position when we calculate its position the same as the real header.
            // So, we want it to be recalculated every time we we change the visibility of the
            // sticky header, to get the correct position.
            if (state.unreadIndicator != null) {
                state.unreadIndicator.destroy();
            }
            if (tab.getCustomView() != null) {
                state.unreadIndicator =
                        new UnreadIndicator(tab.view.findViewById(android.R.id.text1));
            }
            state.unreadIndicator.mNewBadge.setText(state.unreadIndicatorText);
            if (state.shouldAnimateIndicator) {
                state.unreadIndicator.mNewBadge.startAnimation();
            }
        } else {
            if (state.unreadIndicator != null) {
                state.unreadIndicator.destroy();
                state.unreadIndicator = null;
            }
        }

        tab.setContentDescription(contentDescription);
    }

    /**
     * This method sets visibility of the header if this header is sticky to the top of the screen.
     * Does nothing otherwise.
     */
    void setStickyHeaderVisible(boolean isVisible) {}

    /** Adjust the margin of the sticky header. */
    void updateStickyHeaderMargin(int marginValue) {}

    /**
     * Adjust the width of the TabLayout.
     *
     * @param isNarrowWindowOnTablet Whether the window that contains the view is a narrow one on
     *     tablets.
     */
    void updateTabLayoutHeaderWidth(boolean isNarrowWindowOnTablet) {
        if (mTabLayout == null) return;

        MarginLayoutParams layoutParams = (MarginLayoutParams) mTabLayout.getLayoutParams();
        if (!mIsTablet || isNarrowWindowOnTablet) {
            layoutParams.width = LayoutParams.MATCH_PARENT;
        } else {
            layoutParams.width =
                    getResources().getDimensionPixelSize(R.dimen.feed_header_tab_layout_width_max)
                            * 2;
        }
    }
}
