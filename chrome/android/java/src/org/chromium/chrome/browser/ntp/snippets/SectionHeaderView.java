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
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedUma;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
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
public class SectionHeaderView extends LinearLayout implements View.OnClickListener {
    private static final int IPH_TIMEOUT_MS = 10000;
    private static final int ANIMATION_DURATION_MS = 200;

    // Views in the header layout that are set during inflate.
    private TextView mTitleView;
    private TextView mStatusView;
    private ListMenuButton mMenuView;

    // Properties that are set after construction & inflate using setters.
    @Nullable
    private SectionHeader mHeader;

    private boolean mHasMenu;
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
        mStatusView = findViewById(R.id.header_status);
        mMenuView = findViewById(R.id.header_menu);

        // Use the menu instead of the status text when the menu is available from the inflated
        // layout.
        mHasMenu = mMenuView != null;

        if (mHasMenu) {
            mMenuView.setOnClickListener((View v) -> { displayMenu(); });
            int touchPadding;
            // If we are animating padding, add additional touch area around the menu.
            if (mAnimatePaddingWhenDisabled) {
                touchPadding = getResources().getDimensionPixelSize(
                        R.dimen.feed_v2_header_menu_touch_padding);
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
    }

    @Override
    public void onClick(View view) {
        assert mHeader.isExpandable() : "onClick() is called on a non-expandable section header.";
        mHeader.toggleHeader();
        FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_TOGGLED_FEED);
        SuggestionsMetrics.recordExpandableHeaderTapped(mHeader.isExpanded());
        SuggestionsMetrics.recordArticlesListVisible();
    }

    /** @param header The {@link SectionHeader} that holds the data for this class. */
    public void setHeader(SectionHeader header) {
        mHeader = header;
        if (mHeader == null) return;

        // Set visuals with the menu view when present.
        if (mHasMenu) {
            updateVisuals();
            return;
        }

        // Set visuals with the status view when no menu.
        mStatusView.setVisibility(mHeader.isExpandable() ? View.VISIBLE : View.GONE);
        updateVisuals();
        setOnClickListener(mHeader.isExpandable() ? this : null);
    }

    /** Update the header view based on whether the header is expanded and its text contents. */
    public void updateVisuals() {
        if (mHeader == null) return;

        mTitleView.setText(mHeader.getHeaderText());

        if (mHeader.isExpandable()) {
            if (!mHasMenu) {
                mStatusView.setText(
                        mHeader.isExpanded() ? R.string.hide_content : R.string.show_content);
            }
            if (mAnimatePaddingWhenDisabled) {
                int finalHorizontalPadding = 0;
                boolean isClosingHeader = !mHeader.isExpanded();
                if (isClosingHeader) {
                    // If closing header, add additional padding.
                    finalHorizontalPadding = getResources().getDimensionPixelSize(
                            R.dimen.feed_v2_header_menu_disabled_padding);
                } else {
                    // Otherwise, remove the background now.
                    setBackgroundResource(0);
                }
                ValueAnimator animator =
                        ValueAnimator.ofInt(getPaddingLeft(), finalHorizontalPadding);
                animator.addUpdateListener((ValueAnimator animation) -> {
                    int horizontalPadding = (Integer) animation.getAnimatedValue();
                    setPadding(/*left*/ horizontalPadding, getPaddingTop(),
                            /*right*/ horizontalPadding, getPaddingBottom());
                });
                animator.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // If we closed the header, add the hairline after animation.
                        if (isClosingHeader) {
                            setBackgroundResource(R.drawable.hairline_border_card_background);
                        }
                    }
                });
                animator.setDuration(ANIMATION_DURATION_MS);
                animator.start();
            } else {
                setBackgroundResource(
                        mHeader.isExpanded() ? 0 : R.drawable.hairline_border_card_background);
            }
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
        helper.requestShowIPH(new IPHCommandBuilder(mMenuView.getContext().getResources(),
                FeatureConstants.FEED_HEADER_MENU_FEATURE, R.string.ntp_feed_menu_iph,
                R.string.accessibility_ntp_feed_menu_iph)
                                      .setAnchorView(mMenuView)
                                      .setCircleHighlight(true)
                                      .setShouldHighlight(true)
                                      .setDismissOnTouch(false)
                                      .setInsetRect(new Rect(0, 0, 0, -yInsetPx))
                                      .setAutoDismissTimeout(5 * 1000)
                                      .setViewRectProvider(rectProvider)
                                      .build());
    }

    private void displayMenu() {
        FeedUma.recordFeedControlsAction(FeedUma.CONTROLS_ACTION_CLICKED_FEED_HEADER_MENU);

        if (mMenuView == null) {
            assert false : "No menu view to display the menu";
            return;
        }

        ModelList listItems = mHeader.getMenuModelList();
        if (listItems == null) {
            assert false : "No list items model to display the menu";
            return;
        }

        ListMenu.Delegate listMenuDelegate = mHeader.getListMenuDelegate();
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
