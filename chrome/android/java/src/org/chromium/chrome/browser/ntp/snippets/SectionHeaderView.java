// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedUma;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * View for the header of the personalized feed that has a context menu to
 * manage the feed.
 */
public class SectionHeaderView extends LinearLayout {
    private static final int ANIMATION_DURATION_MS = 200;

    /** OnTabSelectedListener that delegates calls to the SectionHeadSelectedListener. */
    private class SectionHeaderTabListener implements TabLayout.OnTabSelectedListener {
        private @Nullable OnSectionHeaderSelectedListener mListener;

        @Override
        public void onTabSelected(TabLayout.Tab tab) {
            if (mListener != null) {
                mListener.onSectionHeaderSelected(tab.getPosition());
            }
        }

        @Override
        public void onTabUnselected(TabLayout.Tab tab) {
            // Do nothing; Not supported.
        }

        @Override
        public void onTabReselected(TabLayout.Tab tab) {
            // Do nothing; Not supported.
        }
    }

    // Views in the header layout that are set during inflate.
    private @Nullable ImageView mVisibilityIndicator;
    private @Nullable TabLayout mTabLayout;
    private @Nullable TextView mTitleView;
    private ListMenuButton mMenuView;

    private @Nullable SectionHeaderTabListener mTabListener;
    private boolean mAnimatePaddingWhenDisabled;

    public SectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        TypedArray attrArray = context.getTheme().obtainStyledAttributes(
                attrs, R.styleable.SectionHeaderView, 0, 0);

        try {
            mAnimatePaddingWhenDisabled = attrArray.getBoolean(
                    R.styleable.SectionHeaderView_animatePaddingWhenDisabled, false);
        } finally {
            attrArray.recycle();
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(R.id.header_title);
        mMenuView = findViewById(R.id.header_menu);
        mVisibilityIndicator = findViewById(R.id.visibility_indicator);
        mTabLayout = findViewById(R.id.tab_list_view);

        if (mTabLayout != null) {
            mTabListener = new SectionHeaderTabListener();
            mTabLayout.addOnTabSelectedListener(mTabListener);
        }

        int touchPadding;
        // If we are animating padding, add additional touch area around the menu.
        if (mAnimatePaddingWhenDisabled) {
            touchPadding =
                    getResources().getDimensionPixelSize(R.dimen.feed_v2_header_menu_touch_padding);
        } else {
            touchPadding = 0;
        }
        post(() -> {
            Rect rect = new Rect();
            mMenuView.getHitRect(rect);

            rect.top -= touchPadding;
            rect.bottom += touchPadding;
            rect.left -= touchPadding;
            rect.right += touchPadding;

            setTouchDelegate(new TouchDelegate(rect, mMenuView));
        });
    }

    /** Updates header text for this view. */
    void setHeaderText(String text) {
        if (mTitleView != null) {
            mTitleView.setText(text);
        }
    }

    /** Adds a blank tab. */
    void addTab() {
        if (mTabLayout != null) {
            mTabLayout.addTab(mTabLayout.newTab());
        }
    }

    /**
     * Set text for the header tab at a particular index to text.
     *
     * Does nothing if index is invalid. Make sure to call addTab() beforehand.
     *
     * @param text Text to set the tab to.
     * @param index Index of the tab to set.
     */
    void setHeaderTextAt(String text, int index) {
        if (mTabLayout != null && mTabLayout.getTabCount() > index && index >= 0) {
            mTabLayout.getTabAt(index).setText(text);
        }
    }

    /**
     * @param index The index of the tab to set as active. Does nothing if index is invalid.
     */
    void setActiveTab(int index) {
        if (mTabLayout != null && mTabLayout.getTabCount() > index && index >= 0) {
            mTabLayout.selectTab(mTabLayout.getTabAt(index));
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
        mMenuView.setOnClickListener((v) -> { displayMenu(listItems, listMenuDelegate); });
    }

    /** Expand the header to indicate the section has been enabled. */
    void expandHeader() {
        if (mAnimatePaddingWhenDisabled) {
            int finalHorizontalPadding = 0;
            setBackgroundResource(0);
            if (mVisibilityIndicator != null) {
                mVisibilityIndicator.setVisibility(View.INVISIBLE);
            }
            ValueAnimator animator = ValueAnimator.ofInt(getPaddingLeft(), finalHorizontalPadding);
            animator.addUpdateListener((ValueAnimator animation) -> {
                int horizontalPadding = (Integer) animation.getAnimatedValue();
                setPadding(/*left*/ horizontalPadding, getPaddingTop(),
                        /*right*/ horizontalPadding, getPaddingBottom());
            });
            animator.setDuration(ANIMATION_DURATION_MS);
            animator.start();
        } else {
            setBackgroundResource(0);
        }
    }

    /** Collapse the header to indicate the section has been disabled. */
    void collapseHeader() {
        if (mAnimatePaddingWhenDisabled) {
            int finalHorizontalPadding = getResources().getDimensionPixelSize(
                    R.dimen.feed_v2_header_menu_disabled_padding);
            ValueAnimator animator = ValueAnimator.ofInt(getPaddingLeft(), finalHorizontalPadding);
            animator.addUpdateListener((ValueAnimator animation) -> {
                int horizontalPadding = (Integer) animation.getAnimatedValue();
                setPadding(/*left*/ horizontalPadding, getPaddingTop(),
                        /*right*/ horizontalPadding, getPaddingBottom());
            });
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    // Add the hairline after animation.
                    setBackgroundResource(R.drawable.hairline_border_card_background);
                    if (mVisibilityIndicator != null) {
                        mVisibilityIndicator.setVisibility(View.VISIBLE);
                    }
                }
            });
            animator.setDuration(ANIMATION_DURATION_MS);
            animator.start();
        } else {
            setBackgroundResource(R.drawable.hairline_border_card_background);
        }
    }

    /** Shows an IPH on the feed header menu button. */
    public void showMenuIph(UserEducationHelper helper) {
        final ViewRectProvider rectProvider = new ViewRectProvider(mMenuView) {
            // ViewTreeObserver.OnPreDrawListener implementation.
            @Override
            public boolean onPreDraw() {
                boolean result = super.onPreDraw();

                int minRectBottomPosPx =
                        getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + mMenuView.getHeight() / 2;
                // Notify that the rectangle is hidden to dismiss the popup if the anchor is
                // positioned too high.
                if (getRect().bottom < minRectBottomPosPx) {
                    notifyRectHidden();
                }

                return result;
            }
        };
        int yInsetPx =
                getResources().getDimensionPixelOffset(R.dimen.text_bubble_menu_anchor_y_inset);
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
                new IPHCommandBuilder(mMenuView.getContext().getResources(),
                        FeatureConstants.FEED_HEADER_MENU_FEATURE, R.string.ntp_feed_menu_iph,
                        R.string.accessibility_ntp_feed_menu_iph)
                        .setAnchorView(mMenuView)
                        .setDismissOnTouch(false)
                        .setInsetRect(new Rect(0, 0, 0, -yInsetPx))
                        .setAutoDismissTimeout(5 * 1000)
                        .setViewRectProvider(rectProvider)
                        // Set clipChildren is important to make sure the bubble does not get
                        // clipped. Set back for better performance during layout.
                        .setOnShowCallback(() -> setClipChildren(false))
                        .setOnDismissCallback(() -> setClipChildren(true))
                        .setHighlightParams(params)
                        .build());
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
                new BasicListMenu(mMenuView.getContext(), listItems, listMenuDelegate);

        ListMenuButtonDelegate delegate = new ListMenuButtonDelegate() {
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
}
