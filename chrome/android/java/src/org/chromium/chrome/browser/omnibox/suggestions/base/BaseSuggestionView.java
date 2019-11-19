// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.support.annotation.IdRes;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.ui.widget.RoundedCornerImageView;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

/**
 * Base layout for common suggestion types. Includes support for a configurable suggestion content
 * and the common suggestion patterns shared across suggestion formats.
 */
public class BaseSuggestionView extends SimpleHorizontalLayoutView {
    protected final ImageView mActionView;
    protected final DecoratedSuggestionView mContentView;

    private SuggestionViewDelegate mDelegate;

    /**
     * Constructs a new suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     */
    public BaseSuggestionView(View view) {
        super(view.getContext());

        TypedValue themeRes = new TypedValue();
        getContext().getTheme().resolveAttribute(R.attr.selectableItemBackground, themeRes, true);
        @DrawableRes
        int selectableBackgroundRes = themeRes.resourceId;

        mContentView = new DecoratedSuggestionView(getContext(), selectableBackgroundRes);
        mContentView.setOnClickListener(v -> mDelegate.onSelection());
        mContentView.setOnLongClickListener(v -> {
            mDelegate.onLongPress();
            return true;
        });
        mContentView.setLayoutParams(LayoutParams.forDynamicView());
        addView(mContentView);

        // Action icons. Currently we only support the Refine button.
        mActionView = new ImageView(getContext());
        mActionView.setBackgroundResource(selectableBackgroundRes);
        mActionView.setClickable(true);
        mActionView.setFocusable(true);
        mActionView.setScaleType(ImageView.ScaleType.CENTER);
        mActionView.setContentDescription(
                getResources().getString(R.string.accessibility_omnibox_btn_refine));
        mActionView.setImageResource(R.drawable.btn_suggestion_refine);
        mActionView.setOnClickListener(v -> mDelegate.onRefineSuggestion());

        mActionView.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_refine_width),
                LayoutParams.MATCH_PARENT));
        addView(mActionView);

        setContentView(view);
    }

    /**
     * Constructs a new suggestion view and inflates supplied layout as the contents view.
     *
     * @param context The context used to construct the suggestion view.
     * @param layoutId Layout ID to be inflated as the contents view.
     */
    public BaseSuggestionView(Context context, @LayoutRes int layoutId) {
        this(LayoutInflater.from(context).inflate(layoutId, null));
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        int contentViewWidth = MeasureSpec.getSize(widthSpec);
        // TODO(ender): Drop this end padding, and expand the icon size by 8dp to ensure it remains
        // centered with the omnibox "Clear" button.
        contentViewWidth -= getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_refine_view_modern_end_padding);
        super.onMeasure(
                MeasureSpec.makeMeasureSpec(contentViewWidth, MeasureSpec.EXACTLY), heightSpec);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // Whenever the suggestion dropdown is touched, we dispatch onGestureDown which is
        // used to let autocomplete controller know that it should stop updating suggestions.
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mDelegate.onGestureDown();
        } else if (ev.getActionMasked() == MotionEvent.ACTION_UP) {
            mDelegate.onGestureUp(ev.getEventTime());
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        if ((!isRtl && KeyNavigationUtil.isGoRight(event))
                || (isRtl && KeyNavigationUtil.isGoLeft(event))) {
            mDelegate.onRefineSuggestion();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void setSelected(boolean selected) {
        mContentView.setSelected(selected);
        mDelegate.onSetUrlToSuggestion();
    }

    /**
     * Set the content view to supplied view.
     *
     * @param view View to be displayed as suggestion content.
     */
    void setContentView(View view) {
        mContentView.setContentView(view);
    }

    /** Sets the delegate for the actions on the suggestion view. */
    void setDelegate(SuggestionViewDelegate delegate) {
        mDelegate = delegate;
    }

    /** Return widget holding suggestion decoration icon. */
    RoundedCornerImageView getSuggestionImageView() {
        return mContentView.getImageView();
    }

    /** Return widget holding action icon. */
    ImageView getActionImageView() {
        return mActionView;
    }

    /**
     * Find content view by view id.
     *
     * Scoped {@link #findViewById(int)} search for the view specified in
     * {@link #setContentView(View)}.
     *
     * @param id View ID of the sought view.
     * @return View with the specified ID or null, if view could not be found.
     */
    public <T extends View> T findContentView(@IdRes int id) {
        return mContentView.findContentView(id);
    }
}
