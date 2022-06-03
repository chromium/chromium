// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.VERTICAL_EXPANDER_CHEVRON;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantChevronStyle;
import org.chromium.components.browser_ui.widget.TintedDrawable;

/**
 * This class provides a vertical expander widget, which allows to toggle between a collapsed and an
 * expanded state of its child views.
 *
 * This widget expects three child views, one for the title, one for the collapsed and another for
 * the expanded state. Each child <strong>must</strong> provide one of the respective layout
 * parameters for disambiguation, otherwise the child won't be added at all!
 */
public class AssistantVerticalExpander extends LinearLayout {
    private final ViewGroup mTitleContainer;
    private final ViewGroup mCollapsedContainer;
    private final ViewGroup mExpandedContainer;
    private final LinearLayout mTitleAndChevronContainer;
    private final View mChevronButton;

    private Callback<Boolean> mOnExpansionStateChangedListener;
    private View mTitleView;
    private View mCollapsedView;
    private View mExpandedView;
    private boolean mExpanded;
    private boolean mFixed;
    private @AssistantChevronStyle int mChevronStyle = AssistantChevronStyle.NOT_SET_AUTOMATIC;

    public AssistantVerticalExpander(Context context, AttributeSet attrs) {
        super(context, attrs);
        setOrientation(VERTICAL);

        mTitleContainer = createChildContainer();
        mChevronButton = createChevron();
        mCollapsedContainer = createChildContainer();
        mExpandedContainer = createChildContainer();

        LinearLayout titleContainer = new LinearLayout(getContext());
        titleContainer.setOrientation(VERTICAL);
        titleContainer.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        titleContainer.addView(mTitleContainer);
        titleContainer.addView(mCollapsedContainer);

        mTitleAndChevronContainer = new LinearLayout(getContext());
        mTitleAndChevronContainer.setOrientation(HORIZONTAL);
        mTitleAndChevronContainer.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mTitleAndChevronContainer.addView(titleContainer,
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        mTitleAndChevronContainer.addView(mChevronButton);
        update();
        addView(mTitleAndChevronContainer);
        addView(mExpandedContainer);

        // Expand/Collapse when user clicks on the title, the chevron, or the collapsed section.
        mTitleAndChevronContainer.setOnClickListener(unusedView -> {
            if (!mFixed) {
                setExpanded(!mExpanded);
            }
        });
    }

    public void setOnExpansionStateChangedListener(Callback<Boolean> listener) {
        mOnExpansionStateChangedListener = listener;
    }

    public void setTitleView(View view, ViewGroup.LayoutParams lp) {
        mTitleView = view;
        mTitleContainer.removeAllViews();
        if (view != null) {
            mTitleContainer.addView(view, lp);
        }
    }

    public void setCollapsedView(View view, ViewGroup.LayoutParams lp) {
        mCollapsedView = view;
        mCollapsedContainer.removeAllViews();
        if (view != null) {
            mCollapsedContainer.addView(view, lp);
        }
        update();
    }

    public void setExpandedView(View view, ViewGroup.LayoutParams lp) {
        mExpandedView = view;
        mExpandedContainer.removeAllViews();
        if (view != null) {
            mExpandedContainer.addView(view, lp);
        }
        update();
    }

    public void setExpanded(boolean expanded) {
        if (expanded == mExpanded) {
            return;
        }

        mExpanded = expanded;
        mChevronButton.setScaleY(mExpanded ? -1 : 1);
        update();
        if (mOnExpansionStateChangedListener != null) {
            mOnExpansionStateChangedListener.onResult(mExpanded);
        }
    }

    /**
     * Allows to hide the expand/collapse chevron, preventing users from expanding or collapsing the
     * expander.
     * @param fixed Whether expanding/collapsing should be disallowed (true) or allowed (false).
     */
    public void setFixed(boolean fixed) {
        if (fixed != mFixed) {
            mFixed = fixed;
            update();
        }
    }

    public void setChevronStyle(@AssistantChevronStyle int chevronStyle) {
        if (chevronStyle != mChevronStyle) {
            mChevronStyle = chevronStyle;
            update();
        }
    }

    public boolean isExpanded() {
        return mExpanded;
    }

    public boolean isFixed() {
        return mFixed;
    }

    public void setCollapsedVisible(boolean visible) {
        mCollapsedContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    public void setExpandedVisible(boolean visible) {
        mExpandedContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    public View getChevronButton() {
        return mChevronButton;
    }
    public View getTitleView() {
        return mTitleView;
    }
    public View getCollapsedView() {
        return mCollapsedView;
    }
    public View getExpandedView() {
        return mExpandedView;
    }
    public View getTitleAndChevronContainer() {
        return mTitleAndChevronContainer;
    }

    /**
     * Creates a default container for children of this widget.
     */
    private ViewGroup createChildContainer() {
        FrameLayout container = new FrameLayout(getContext());
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        container.setLayoutParams(lp);
        return container;
    }

    private View createChevron() {
        TintedDrawable chevron = TintedDrawable.constructTintedDrawable(
                getContext(), R.drawable.ic_expand_more_black_24dp, R.color.default_icon_color);

        ImageView view = new ImageView(getContext());
        view.setImageDrawable(chevron);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.CENTER_VERTICAL;
        view.setLayoutParams(lp);
        view.setTag(VERTICAL_EXPANDER_CHEVRON);
        return view;
    }

    private void update() {
        switch (mChevronStyle) {
            case AssistantChevronStyle.NOT_SET_AUTOMATIC:
                mChevronButton.setVisibility(
                        !mFixed && mExpandedView != null ? View.VISIBLE : View.GONE);
                break;
            case AssistantChevronStyle.ALWAYS:
                mChevronButton.setVisibility(View.VISIBLE);
                break;
            case AssistantChevronStyle.NEVER:
                mChevronButton.setVisibility(View.GONE);
                break;
        }

        if (mExpandedView != null) {
            mExpandedView.setVisibility(mExpanded ? VISIBLE : GONE);
        }
        if (mCollapsedView != null) {
            mCollapsedView.setVisibility(mExpanded ? GONE : VISIBLE);
        }
    }
}
