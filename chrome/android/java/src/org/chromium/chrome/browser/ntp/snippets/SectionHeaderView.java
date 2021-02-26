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
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
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

    // Views in the header layout that are set during inflate.
    private TextView mTitleView;
    private ListMenuButton mMenuView;

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
    public void setHeaderText(String text) {
        mTitleView.setText(text);
    }

    /** Sets the delegate for the gear/settings icon. */
    public void setMenuDelegate(ModelList listItems, ListMenu.Delegate listMenuDelegate) {
        mMenuView.setOnClickListener((v) -> { displayMenu(listItems, listMenuDelegate); });
    }

    /** Expand the header to indicate the section has been enabled. */
    public void expandHeader() {
        if (mAnimatePaddingWhenDisabled) {
            int finalHorizontalPadding = 0;
            setBackgroundResource(0);
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
    public void collapseHeader() {
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
        PulseDrawable pulseDrawable = PulseDrawable.createCustomCircle(
                mMenuView.getContext(), new PulseDrawable.Bounds() {
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
                        .setCircleHighlight(true)
                        .setShouldHighlight(true)
                        .setDismissOnTouch(false)
                        .setInsetRect(new Rect(0, 0, 0, -yInsetPx))
                        .setAutoDismissTimeout(5 * 1000)
                        .setViewRectProvider(rectProvider)
                        // Set clipChildren is important to make sure the bubble does not get
                        // clipped. Set back for better performance during layout.
                        .setOnShowCallback(() -> setClipChildren(false))
                        .setOnDismissCallback(() -> setClipChildren(true))
                        .setHighlighter(pulseDrawable)
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
