// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.res.Resources;
import android.support.design.widget.AppBarLayout;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.coordinator.CoordinatorLayoutForPointer;
import org.chromium.chrome.browser.ui.styles.ChromeColors;
import org.chromium.chrome.tab_ui.R;

// The view of the tasks surface.
class TasksView extends CoordinatorLayoutForPointer {
    private final Context mContext;
    private FrameLayout mBodyViewContainer;
    private FrameLayout mCarouselTabSwitcherContainer;
    private AppBarLayout mHeaderView;
    private View mSearchBox;
    private TextView mSearchBoxText;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCarouselTabSwitcherContainer =
                (FrameLayout) findViewById(R.id.carousel_tab_switcher_container);
        mSearchBox = findViewById(R.id.search_box);
        mHeaderView = (AppBarLayout) findViewById(R.id.task_surface_header);
        AppBarLayout.LayoutParams layoutParams =
                (AppBarLayout.LayoutParams) mSearchBox.getLayoutParams();
        layoutParams.setScrollFlags(AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL);
        mSearchBoxText = (TextView) mSearchBox.findViewById(R.id.search_box_text);
    }

    ViewGroup getCarouselTabSwitcherContainer() {
        return mCarouselTabSwitcherContainer;
    }

    ViewGroup getBodyViewContainer() {
        return findViewById(R.id.tasks_surface_body);
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
     * Set the given listener for the fake search box.
     * @param listener The given listener.
     */
    void setFakeSearchBoxClickListener(@Nullable View.OnClickListener listener) {
        mSearchBoxText.setOnClickListener(listener);
    }

    /**
     * Set the given watcher for the fake search box.
     * @param textWatcher The given {@link TextWatcher}.
     */
    void setFakeSearchBoxTextWatcher(TextWatcher textWatcher) {
        mSearchBoxText.addTextChangedListener(textWatcher);
    }

    /**
     * Set the visibility of the fake search box.
     * @param isVisible Whether it's visible.
     */
    void setFakeSearchBoxVisibility(boolean isVisible) {
        mSearchBox.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the visibility of the voice recognition button.
     * @param isVisible Whether it's visible.
     */
    void setVoiceRecognitionButtonVisibility(boolean isVisible) {
        findViewById(R.id.voice_search_button).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the voice recognition button click listener.
     * @param listener The given listener.
     */
    void setVoiceRecognitionButtonClickListener(@Nullable View.OnClickListener listener) {
        findViewById(R.id.voice_search_button).setOnClickListener(listener);
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
        mSearchBox.setBackgroundResource(
                isIncognito ? R.drawable.fake_search_box_bg_incognito : R.drawable.ntp_search_box);
        int hintTextColor = isIncognito
                ? ApiCompatibilityUtils.getColor(resources, R.color.locationbar_light_hint_text)
                : ApiCompatibilityUtils.getColor(resources, R.color.locationbar_dark_hint_text);
        mSearchBoxText.setHintTextColor(hintTextColor);
    }
}
