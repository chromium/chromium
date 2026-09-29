// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.content.Context;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.TooltipCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Controls the fragment-scoped search button and {@link SearchView} in the detailed pane title when
 * Chrome Android Settings runs in a tab ({@code SettingsInTab}).
 */
@NullMarked
class DetailedPaneSearchController {
    private static final LinearLayout.LayoutParams LAYOUT_CENTER_VERTICAL;

    static {
        LAYOUT_CENTER_VERTICAL = new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        LAYOUT_CENTER_VERTICAL.gravity = Gravity.CENTER_VERTICAL;
    }

    private final Context mContext;
    private final LinearLayout mContainer;
    private final MultiColumnSettings mMultiColumnSettings;
    private final @Nullable Runnable mOnSearchVisibilityChanged;

    private @Nullable View mActiveTitleView;
    private @Nullable ChromeImageButton mActiveSearchButton;
    private @Nullable SearchView mActiveSearchView;
    private @Nullable OnBackPressedCallback mBackPressedCallback;

    DetailedPaneSearchController(
            Context context,
            LinearLayout container,
            MultiColumnSettings multiColumnSettings,
            @Nullable Runnable onSearchVisibilityChanged) {
        mContext = context;
        mContainer = container;
        mMultiColumnSettings = multiColumnSettings;
        mOnSearchVisibilityChanged = onSearchVisibilityChanged;
    }

    /**
     * Initializes or updates search UI according to whether the detail fragment implements {@link
     * SearchViewProvider}.
     *
     * @param titleView The active title view to toggle when search opens or closes.
     * @param detailFragment The active fragment in R.id.preferences_detail.
     */
    void onTitlesUpdated(@Nullable View titleView, @Nullable Fragment detailFragment) {
        if (titleView == null
                || !(detailFragment instanceof SearchViewProvider searchViewProvider)) {
            return;
        }

        var titleParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        titleParams.gravity = Gravity.CENTER_VERTICAL;
        titleView.setLayoutParams(titleParams);

        var searchButton = new ChromeImageButton(mContext);
        searchButton.setImageResource(R.drawable.ic_search_24dp);
        searchButton.setScaleType(ImageView.ScaleType.CENTER);
        searchButton.setBackgroundResource(R.drawable.default_icon_background);
        int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
        searchButton.setMinimumWidth(minTouchTargetPx);
        searchButton.setMinimumHeight(minTouchTargetPx);
        TooltipCompat.setTooltipText(searchButton, mContext.getString(R.string.search));
        searchButton.setContentDescription(mContext.getString(R.string.search));
        searchButton.setLayoutParams(new LinearLayout.LayoutParams(LAYOUT_CENTER_VERTICAL));

        var searchView = new SearchView(mContext);
        var searchViewParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        searchViewParams.gravity = Gravity.CENTER_VERTICAL;
        searchView.setLayoutParams(searchViewParams);
        // Do not set a max width. The HorizontalScrollView measures its content with an
        // unbounded width, so the SearchView would grow to its max width and push the
        // close button off screen. Its fillViewport attribute stretches the SearchView to
        // the pane width instead.
        searchView.setVisibility(View.GONE);
        if (TextUtils.isEmpty(searchView.getQueryHint())) {
            searchView.setQueryHint(mContext.getString(R.string.search));
        }
        View searchPlate = searchView.findViewById(R.id.search_plate);
        if (searchPlate != null) {
            // The small search plate intentionally has no background on tablet/desktop,
            // similar to its appearance on mobile.
            searchPlate.setBackground(null);
        }

        mActiveTitleView = titleView;
        mActiveSearchButton = searchButton;
        mActiveSearchView = searchView;

        searchButton.setOnClickListener(v -> openSearch());

        searchView.setOnCloseListener(
                () -> {
                    closeSearch();
                    return false;
                });
        searchViewProvider.setSearchViewObserver(
                (visible) -> {
                    if (!visible) {
                        closeSearch();
                    }
                });
        // Must be called after configuring listeners and setting the observer,
        // so that initSearchView (via SearchUtils) receives the observer and does
        // not have its close listener overwritten.
        searchViewProvider.initSearchView(searchView);

        View.OnKeyListener escKeyListener =
                (v, keyCode, event) -> {
                    if (keyCode == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
                        if (event.getAction() == KeyEvent.ACTION_DOWN
                                && event.getRepeatCount() == 0) {
                            handleBackAction();
                        }
                        return true;
                    }
                    return false;
                };
        searchView.setOnKeyListener(escKeyListener);
        View searchSrcTextView = searchView.requireViewById(R.id.search_src_text);
        searchSrcTextView.setOnKeyListener(escKeyListener);

        mContainer.addView(searchButton);
        mContainer.addView(searchView);
    }

    /**
     * Resets search state, detaching active search views, resetting cached view references,
     * disabling the back-press callback, and notifying the visibility callback.
     */
    void reset() {
        closeSearch();
        if (mActiveSearchButton != null) {
            mContainer.removeView(mActiveSearchButton);
            mActiveSearchButton = null;
        }
        if (mActiveSearchView != null) {
            mContainer.removeView(mActiveSearchView);
            mActiveSearchView = null;
        }
        mActiveTitleView = null;
        if (mBackPressedCallback != null) {
            mBackPressedCallback.setEnabled(false);
        }
        if (mOnSearchVisibilityChanged != null) {
            mOnSearchVisibilityChanged.run();
        }
    }

    /** Opens the search view, hides the title and search icon, and requests focus. */
    void openSearch() {
        if (mActiveTitleView != null) {
            mActiveTitleView.setVisibility(View.GONE);
        }
        if (mActiveSearchButton != null) {
            mActiveSearchButton.setVisibility(View.GONE);
        }
        if (mActiveSearchView != null) {
            mActiveSearchView.setVisibility(View.VISIBLE);
            mActiveSearchView.setIconified(false);
            mActiveSearchView.requestFocus();
            View searchSrcTextView = mActiveSearchView.requireViewById(R.id.search_src_text);
            SettingsMenuHelper.requestAccessibilityFocus(searchSrcTextView);
        }
        ensureBackPressedCallback();
        if (mBackPressedCallback != null) {
            mBackPressedCallback.setEnabled(true);
        }
        if (mOnSearchVisibilityChanged != null) {
            mOnSearchVisibilityChanged.run();
        }
    }

    /** Closes the search view, restores the title and search icon, and clears focus. */
    void closeSearch() {
        if (!isSearchOpen()) {
            return;
        }

        if (mActiveSearchView != null) {
            mActiveSearchView.clearFocus();
            mActiveSearchView.setVisibility(View.GONE);
            mActiveSearchView.setQuery("", false);
            mActiveSearchView.setIconified(true);
        }
        if (mActiveTitleView != null) {
            mActiveTitleView.setVisibility(View.VISIBLE);
        }
        if (mActiveSearchButton != null) {
            mActiveSearchButton.setVisibility(View.VISIBLE);
        }
        if (mBackPressedCallback != null) {
            mBackPressedCallback.setEnabled(false);
        }
        if (mOnSearchVisibilityChanged != null) {
            mOnSearchVisibilityChanged.run();
        }
    }

    /** Returns whether the detailed pane search view is currently visible. */
    boolean isSearchOpen() {
        return mActiveSearchView != null && mActiveSearchView.getVisibility() == View.VISIBLE;
    }

    /**
     * Intercepts back action (e.g. Escape key or back gesture) to close search if open.
     *
     * @return True if consumed by closing search, false otherwise.
     */
    boolean handleBackAction() {
        if (isSearchOpen()) {
            closeSearch();
            return true;
        }
        return false;
    }

    /** Returns true if a search button is currently present in the title container. */
    boolean hasSearchButton() {
        return mActiveSearchButton != null;
    }

    /** Returns the horizontal pixel offset required to accommodate the search button ripple. */
    int getSearchButtonOffsetPx() {
        if (mActiveSearchButton == null) {
            return 0;
        }
        int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
        int iconWidthPx = minTouchTargetPx / 2;
        if (mActiveSearchButton.getDrawable() != null) {
            iconWidthPx = mActiveSearchButton.getDrawable().getIntrinsicWidth();
        }
        return (minTouchTargetPx - iconWidthPx) / 2;
    }

    private void ensureBackPressedCallback() {
        if (mBackPressedCallback != null) {
            return;
        }

        FragmentActivity activity = mMultiColumnSettings.getActivity();
        if (activity == null) {
            return;
        }

        mBackPressedCallback =
                new OnBackPressedCallback(/* enabled= */ false) {
                    @Override
                    public void handleOnBackPressed() {
                        closeSearch();
                    }
                };
        activity.getOnBackPressedDispatcher()
                .addCallback(mMultiColumnSettings, mBackPressedCallback);
    }

    private int getDimenPx(int res) {
        return mContext.getResources().getDimensionPixelSize(res);
    }
}
