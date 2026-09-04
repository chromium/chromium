// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.text.LineBreaker;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.TooltipCompat;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.List;

/**
 * Observes MultiColumnSettings events, and updates the SettingsActivity's title and its detailed
 * pane titles.
 */
@NullMarked
class MultiColumnTitleUpdater implements MultiColumnSettings.Observer {

    private static final LinearLayout.LayoutParams LAYOUT_CENTER_VERTICAL;

    private static final String TAG = "MultiColTitleUpdater";

    private static final String KEY_FIRST_VISIBLE_INDEX = "first_visible_index";
    private static final String KEY_CACHED_DEEP_LINK_PATH = "cached_deep_link_path";

    static {
        LAYOUT_CENTER_VERTICAL = new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        LAYOUT_CENTER_VERTICAL.gravity = Gravity.CENTER_VERTICAL;
    }

    /** Displays one component of the detailed pane fragment stack. */
    private static class DetailedTitle extends AppCompatTextView {
        private final Callback<String> mSetter =
                (title) -> {
                    if (title == null) {
                        title = "";
                    }
                    setText(title);
                };

        private @Nullable MonotonicObservableSupplier<String> mSupplier;

        DetailedTitle(Context context) {
            super(context);

            // Use the same TextAppearance with the main settings title.
            setTextAppearance(R.style.TextAppearance_Headline_Primary);
            ViewCompat.setAccessibilityHeading(this, true);
            setFocusable(true);
        }

        void setSupplier(@Nullable MonotonicObservableSupplier<String> supplier) {
            if (mSupplier == supplier) {
                return;
            }
            if (mSupplier != null) {
                mSupplier.removeObserver(mSetter);
            }
            mSupplier = supplier;
            if (mSupplier != null) {
                mSupplier.addSyncObserverAndCallIfNonNull(mSetter);
            }
        }
    }

    private final MultiColumnSettings mMultiColumnSettings;

    // Refers original SettingsActivity practically.
    private final Context mContext;

    /** Container to list the TextViews of fragment stack. */
    private final LinearLayout mContainer;

    /** Delegates the title settings to the callback. */
    private final Callback<String> mMainTitleSetter;

    /** Callback invoked when a title text is tapped. */
    private final Callback<@Nullable String> mTitleTapCallback;

    private boolean mMainMenuShown;
    private boolean mHasBackButton;
    private boolean mHasSearchButton;

    private @Nullable View mActiveTitleView;
    private @Nullable ChromeImageButton mActiveSearchButton;
    private @Nullable SearchView mActiveSearchView;
    private @Nullable OnBackPressedCallback mBackPressedCallback;

    /**
     * The index of the first title to show. Used to skip displaying the titles preceding {@code
     * Search results} when search is going on.
     *
     * <p>Example: if search starts with the title text {@code Payment methods > Payment apps >
     * Search results}, |mFirstVisibleTitleIndex| is set to 2 so that the displayed text will be
     * just {@code Search results > ..} from that point on. Once search is over, the variable is set
     * back to 0 and the displayed text becomes {@code Payment method > Payment apps} again.
     */
    private int mFirstVisibleTitleIndex;

    /**
     * Keeps tracking the current main page title supplier. Null if not tracking, e.g. in two pane
     * mode.
     */
    private @Nullable MonotonicObservableSupplier<String> mCurrentPageTitle;

    private final @Nullable List<SettingsIndexData.Entry> mInitialBreadcrumbPath;
    private @Nullable List<SettingsIndexData.Entry> mCachedDeepLinkPath;
    private final @Nullable Runnable mOnSearchVisibilityChanged;

    MultiColumnTitleUpdater(
            @Nullable Bundle savedInstanceState,
            MultiColumnSettings multiColumnSettings,
            LinearLayout container,
            Callback<String> mainTitleSetter,
            Callback<@Nullable String> titleTapCallback,
            @Nullable List<SettingsIndexData.Entry> initialBreadcrumbPath,
            @Nullable Runnable onSearchVisibilityChanged) {
        mMultiColumnSettings = multiColumnSettings;
        mContext = container.getContext();
        mContainer = container;
        mMainTitleSetter = mainTitleSetter;
        mTitleTapCallback = titleTapCallback;
        mInitialBreadcrumbPath = initialBreadcrumbPath;
        mOnSearchVisibilityChanged = onSearchVisibilityChanged;

        restoreInstanceState(savedInstanceState);

        final int originalHeight = getDimenPx(R.dimen.settings_detailed_title_height);

        // TODO(crbug.com/480084682): Remove this listener after search is enabled, since
        //     title views will be horizontally scrollable.
        mContainer.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> {
                    int actualHeight = bottom - top;

                    // If actual height is bigger than the original one, some text view
                    // is overflown and being wrapped. In the case, we relayout the view
                    // by evenly splitting the components (to avoid only the last component
                    // has very narrow width space and shrunk in very weird way).
                    if (actualHeight > originalHeight) {
                        for (int i = mContainer.getChildCount() - 1; i >= 0; --i) {
                            if (mContainer.getChildAt(i) instanceof DetailedTitle title) {
                                LinearLayout.LayoutParams params =
                                        (LinearLayout.LayoutParams) title.getLayoutParams();
                                // not to relayout when unneeded, check the weight.
                                if (params.weight == 1f) {
                                    // DetailedTitle views leading this element should have 1f
                                    // already.
                                    break;
                                }
                                params.weight = 1f;
                                params.width = 0;
                                title.setLayoutParams(params);
                            }
                        }
                    } else if (!mHasSearchButton) {
                        // When mHasSearchButton is true, the title view must keep weight=1f to
                        // keep the search button right-justified. Otherwise, reset weight to 0f
                        // and width to WRAP_CONTENT when not overflowing.
                        // Note: we cannot traverse in the reverse order here unlike above,
                        // because a new view may be just added and so even if weight=0 view
                        // is found, there may be weight!=0 views in leading components.
                        for (int i = 0; i < mContainer.getChildCount(); ++i) {
                            if (mContainer.getChildAt(i) instanceof DetailedTitle title) {
                                LinearLayout.LayoutParams params =
                                        (LinearLayout.LayoutParams) title.getLayoutParams();
                                // not to relayout when unneeded, check the weight.
                                if (params.weight != 0f) {
                                    params.weight = 0f;
                                    params.width = LinearLayout.LayoutParams.WRAP_CONTENT;
                                    title.setLayoutParams(params);
                                }
                            }
                        }
                    }
                });
    }

    @Override
    public void onTitleUpdated() {
        updateMainTitle();
        updateDetailedPageTitle();
    }

    @Override
    public void onSlideStateUpdated(int newState) {
        boolean prevMainMenuShown = mMainMenuShown;
        mMainMenuShown =
                newState == MultiColumnSettings.SlideState.CLOSING
                        || newState == MultiColumnSettings.SlideState.CLOSED;
        if (prevMainMenuShown != mMainMenuShown) {
            updateMainTitle();
        }
    }

    private void restoreInstanceState(@Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            mFirstVisibleTitleIndex = savedInstanceState.getInt(KEY_FIRST_VISIBLE_INDEX);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                mCachedDeepLinkPath =
                        savedInstanceState.getParcelableArrayList(
                                KEY_CACHED_DEEP_LINK_PATH, SettingsIndexData.Entry.class);
            } else {
                @SuppressWarnings("deprecation")
                ArrayList<SettingsIndexData.Entry> legacyList =
                        savedInstanceState.getParcelableArrayList(KEY_CACHED_DEEP_LINK_PATH);
                mCachedDeepLinkPath = legacyList;
            }
        } else {
            if (mInitialBreadcrumbPath != null) {
                mCachedDeepLinkPath = new ArrayList<>(mInitialBreadcrumbPath);
            }
        }
    }

    private void updateMainTitle() {
        // Unset if needed, first.
        if (mCurrentPageTitle != null) {
            mCurrentPageTitle.removeObserver(mMainTitleSetter);
        }

        var titles = mMultiColumnSettings.getTitles();
        if (mMultiColumnSettings.isTwoColumn() || titles.isEmpty() || mMainMenuShown) {
            // In the two pane mode, the main title is always "Settings".
            mMainTitleSetter.onResult(mContext.getString(R.string.settings));
            mCurrentPageTitle = null;
        } else {
            // Otherwise, use the last fragment, because it is what a user is seeing.
            mCurrentPageTitle = titles.get(titles.size() - 1).titleSupplier;
            mCurrentPageTitle.addSyncObserverAndCallIfNonNull(mMainTitleSetter);
        }
    }

    /** Set the index of the first title to show. Non-zero when search is on. */
    public void setFirstVisibleTitleIndex(int i) {
        mFirstVisibleTitleIndex = i;
    }

    private List<MultiColumnSettings.Title> initTitlesList() {
        List<MultiColumnSettings.Title> navigatedTitles = mMultiColumnSettings.getTitles();

        // If the user was in search mode (which sets mFirstVisibleTitleIndex > 0 to hide pre-search
        // ancestor breadcrumbs) and selects a new category from MainSettings, the detail fragment
        // stack is replaced and mFirstVisibleTitleIndex becomes out-of-bounds. Reset it to 0 so the
        // new section's title is displayed. http://crbug.com/541086963
        if (mFirstVisibleTitleIndex >= navigatedTitles.size()) {
            mFirstVisibleTitleIndex = 0;
        }

        if (mFirstVisibleTitleIndex == 0) {
            if (navigatedTitles.isEmpty()) {
                mCachedDeepLinkPath = null;
            } else if (navigatedTitles.size() == 1) {
                Fragment currentFragment =
                        mMultiColumnSettings
                                .getChildFragmentManager()
                                .findFragmentById(R.id.preferences_detail);

                assertNonNull(currentFragment);

                boolean isMatch = false;
                if (mInitialBreadcrumbPath != null && !mInitialBreadcrumbPath.isEmpty()) {
                    String targetClass =
                            mInitialBreadcrumbPath.get(mInitialBreadcrumbPath.size() - 1).fragment;
                    isMatch = TextUtils.equals(currentFragment.getClass().getName(), targetClass);
                }

                mCachedDeepLinkPath = isMatch ? new ArrayList<>(mInitialBreadcrumbPath) : null;
            }

            if (mCachedDeepLinkPath != null && mCachedDeepLinkPath.size() > 1) {
                List<MultiColumnSettings.Title> splicedTitles = new ArrayList<>();

                // We are only looking for the ancestors, excluding the current page title.
                int numAncestors = mCachedDeepLinkPath.size() - 1;

                for (int i = 0; i < numAncestors; i++) {
                    SettingsIndexData.Entry entry = mCachedDeepLinkPath.get(i);
                    SettableMonotonicObservableSupplier<String> titleSupplier =
                            ObservableSuppliers.createMonotonic();
                    assertNonNull(entry.title);
                    titleSupplier.set(entry.title);

                    MultiColumnSettings.Title syntheticTitle =
                            new MultiColumnSettings.Title(entry.id, titleSupplier, -1, null);

                    splicedTitles.add(syntheticTitle);
                }

                splicedTitles.addAll(navigatedTitles);
                return splicedTitles;
            }
        }

        return navigatedTitles;
    }

    private void updateDetailedPageTitle() {
        // Reset the current title items if exists.
        for (int i = 0; i < mContainer.getChildCount(); ++i) {
            View view = mContainer.getChildAt(i);
            if (view instanceof DetailedTitle detailedTitle) {
                detailedTitle.setSupplier(null);
            }
        }
        mContainer.removeAllViews();
        mActiveTitleView = null;
        mActiveSearchButton = null;
        mActiveSearchView = null;
        if (mBackPressedCallback != null) {
            mBackPressedCallback.setEnabled(false);
        }
        if (mOnSearchVisibilityChanged != null) {
            mOnSearchVisibilityChanged.run();
        }

        List<MultiColumnSettings.Title> titles = initTitlesList();

        // Padding for the chevron separator.
        int paddingPx = getDimenPx(R.dimen.settings_detailed_title_padding);

        float scaleX = LocalizationUtils.isLayoutRtl() ? -1f : 1f;

        int prevIndex = titles.size() - 2;
        mHasBackButton = SettingsInTab.isEnabled() && prevIndex >= mFirstVisibleTitleIndex;
        // Do not show the back button if the previous title is hidden (e.g. Search results).
        if (mHasBackButton) {
            // Set up a back button to go to the section for the previous title.
            // TODO(crbug.com/545663479): Move this back button out of the containing scrollView,
            // as we always want to show it, even if we have to scroll the title text.
            var prevTitle = titles.get(prevIndex);
            var backButton = new ChromeImageButton(mContext);
            // Assign a stable resource ID so UI tests and accessibility tools
            // can directly look up the title back button without looping over
            // child views.
            backButton.setId(R.id.back_button);
            backButton.setImageResource(R.drawable.ic_arrow_back_24dp);
            // Ensure icon isn't stretched by the larger touch target.
            backButton.setScaleType(ImageView.ScaleType.CENTER);
            // Provide material design circular hover highlight and ripple. Note that this makes
            // the required size of the button much larger than the icon.
            backButton.setBackgroundResource(R.drawable.default_icon_background);
            // Ensure size is large enough for touch accessibility.
            int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
            backButton.setMinimumWidth(minTouchTargetPx);
            backButton.setMinimumHeight(minTouchTargetPx);
            var layoutParams = new LinearLayout.LayoutParams(LAYOUT_CENTER_VERTICAL);
            backButton.setLayoutParams(layoutParams);
            backButton.setOnClickListener(
                    (View v) -> {
                        if (isSearchOpen()) {
                            closeSearch();
                        } else {
                            navigateToTitle(prevTitle, prevIndex);
                        }
                    });
            // Set both tooltip and accessibility content description.
            TooltipCompat.setTooltipText(backButton, mContext.getString(R.string.back));
            backButton.setContentDescription(mContext.getString(R.string.back));
            mContainer.addView(backButton);
        }

        // SettingsInTab only shows the last title, not the full breadcrumb path.
        int startIndex =
                SettingsInTab.isEnabled()
                        ? Math.max(mFirstVisibleTitleIndex, titles.size() - 1)
                        : mFirstVisibleTitleIndex;
        DetailedTitle lastTitleView = null;
        for (int i = startIndex; i < titles.size(); ++i) {
            if (i != startIndex) {
                // '>' separator.
                var view = new ImageView(mContext);
                view.setPadding(paddingPx, 0, paddingPx, 0);
                view.setImageResource(R.drawable.chevron_right);
                view.setScaleX(scaleX);
                // Passed instance is owned by the view, so create the new instance.
                view.setLayoutParams(new LinearLayout.LayoutParams(LAYOUT_CENTER_VERTICAL));
                mContainer.addView(view);
            }
            var view = new DetailedTitle(mContext);
            var title = titles.get(i);
            view.setSupplier(title.titleSupplier);
            view.setGravity(Gravity.CENTER_VERTICAL);
            // Passed instance is owned by the view, so create the new instance.
            view.setLayoutParams(new LinearLayout.LayoutParams(LAYOUT_CENTER_VERTICAL));
            view.setBreakStrategy(LineBreaker.BREAK_STRATEGY_BALANCED);

            if (i < titles.size() - 1) {
                final int finalIndex = i;
                view.setOnClickListener((View v) -> navigateToTitle(title, finalIndex));
            }

            mContainer.addView(view);
            lastTitleView = view;
        }

        mHasSearchButton = false;
        if (SettingsInTab.isEnabled() && lastTitleView != null) {
            Fragment detailFragment =
                    mMultiColumnSettings
                            .getChildFragmentManager()
                            .findFragmentById(R.id.preferences_detail);
            if (detailFragment instanceof SearchViewProvider searchViewProvider) {
                mHasSearchButton = true;
                final DetailedTitle titleView = lastTitleView;
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
                searchView.setMaxWidth(Integer.MAX_VALUE);
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

                // TODO(crbug.com/557197237): Move search view visibility handling and key
                // processing to a separate class.
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
        }

        maybeUpdateMargins();

        // Make the last-added/tapped one visible after adding titles.
        if (mContainer.getParent() instanceof HorizontalScrollView scrollView) {
            scrollView.post(() -> scrollView.fullScroll(HorizontalScrollView.FOCUS_RIGHT));
        }
    }

    private void openSearch() {
        assert SettingsInTab.isEnabled();

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

    private void closeSearch() {
        assert SettingsInTab.isEnabled();

        if (!isSearchOpen()) return;

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

    /** Returns whether the search view in the detailed pane title is open. */
    public boolean isSearchOpen() {
        return mActiveSearchView != null && mActiveSearchView.getVisibility() == View.VISIBLE;
    }

    /**
     * Handles back action (e.g. back press or Escape key). Closes search if open.
     *
     * @return True if back was consumed by closing search, false otherwise.
     */
    public boolean handleBackAction() {
        if (isSearchOpen()) {
            closeSearch();
            return true;
        }
        return false;
    }

    private void ensureBackPressedCallback() {
        assert SettingsInTab.isEnabled();

        // Nothing to do if callback is already set.
        if (mBackPressedCallback != null) return;

        // This method can be called asynchronously from posted tasks.
        FragmentActivity activity = mMultiColumnSettings.getActivity();
        if (activity == null) return;

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

    private void navigateToTitle(MultiColumnSettings.Title title, int index) {
        assert mMultiColumnSettings != null;
        // Note: The current getBackStackEntryCount and recorded backStackCount
        // can be same, e.g., if the user tabs the last component of the
        // detailed title.
        if (title.backStackCount >= 0) {
            if (mMultiColumnSettings.getChildFragmentManager().getBackStackEntryCount()
                    > title.backStackCount) {
                var entry =
                        mMultiColumnSettings
                                .getChildFragmentManager()
                                .getBackStackEntryAt(title.backStackCount);
                mMultiColumnSettings
                        .getChildFragmentManager()
                        .popBackStack(entry.getId(), FragmentManager.POP_BACK_STACK_INCLUSIVE);
                mTitleTapCallback.onResult(entry.getName());
            }
        } else {
            if (mCachedDeepLinkPath != null) {
                SettingsIndexData.Entry entry = mCachedDeepLinkPath.get(index);
                launchFragment(entry);
            }
        }
    }

    /**
     * Navigates to a specific parent fragment when its synthetic breadcrumb is clicked.
     *
     * <p>Instantiates the target fragment using the args stored in the {@link
     * SettingsIndexData.Entry}, and replaces the current detail pane.
     *
     * @param entry The index entry containing the fragment class and arguments.
     */
    private void launchFragment(SettingsIndexData.Entry entry) {
        if (entry.fragment == null) return;

        try {
            FragmentManager fm = mMultiColumnSettings.getChildFragmentManager();

            if (fm.getBackStackEntryCount() > 0) {
                fm.popBackStackImmediate(
                        fm.getBackStackEntryAt(0).getId(),
                        FragmentManager.POP_BACK_STACK_INCLUSIVE);
            }

            mCachedDeepLinkPath = null;

            Fragment f = Fragment.instantiate(mContext, entry.fragment, entry.extras);
            fm.beginTransaction()
                    .setReorderingAllowed(true)
                    .replace(R.id.preferences_detail, f)
                    .commitNow();

        } catch (Exception e) {
            Log.e(TAG, "Failed to launch breadcrumb fragment: " + entry.fragment, e);
        }
    }

    @Override
    public void onHeaderLayoutUpdated() {
        updateMainTitle();

        updateDetailedPageTitle();

        if (!mMultiColumnSettings.isTwoColumn()) {
            // In the single pane mode, do not show the detailed title.
            mContainer.setVisibility(View.GONE);
            return;
        }

        // Enable detailed page title.
        mContainer.setVisibility(View.VISIBLE);

        maybeUpdateMargins();
    }

    @Override
    public void onDetailLayoutUpdated() {
        maybeUpdateMargins();
    }

    /**
     * Updates the start and end margins of the title scroll view. This method has extra null checks
     * so it can be called before the layout is fully inflated and in unit tests.
     */
    private void maybeUpdateMargins() {
        View detailView = mMultiColumnSettings.getDetailView();
        if (detailView == null) return;
        // Check detailView width because recyclerView might not have completed layout during
        // fragment transitions (e.g. screen rotation).
        int widthPx = detailView.getWidth();
        if (widthPx == 0) return;

        int maxDetailWidthPx = getDimenPx(R.dimen.settings_min_multi_column_screen_width);
        int minPaddingPx = getDimenPx(R.dimen.settings_multi_column_pane_gap);
        int startMargin = getDimenPx(R.dimen.settings_detailed_title_start_margin);
        int excessPx = widthPx - maxDetailWidthPx - minPaddingPx * 2;
        int offsetX = minPaddingPx + (excessPx > 0 ? excessPx / 2 : 0);

        // Shift titleScrollView left when the back button is shown so that the extra space
        // for the button's material design ripple background fits inside titleScrollView without
        // being clipped on the left edge.
        int backButtonOffsetPx = 0;
        if (mHasBackButton) {
            assert mContainer.getChildCount() > 0;
            assert mContainer.getChildAt(0) instanceof ChromeImageButton;
            var backButton = (ChromeImageButton) mContainer.getChildAt(0);
            assertNonNull(backButton.getDrawable());
            int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
            int iconWidthPx = backButton.getDrawable().getIntrinsicWidth();
            backButtonOffsetPx = (minTouchTargetPx - iconWidthPx) / 2;
        }

        // Shift titleScrollView right when the search button is shown so that the extra space
        // for the button's material design ripple background fits inside titleScrollView without
        // being clipped on the right edge.
        int searchButtonOffsetPx = 0;
        if (mHasSearchButton) {
            int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
            int iconWidthPx = minTouchTargetPx / 2;
            for (int i = 0; i < mContainer.getChildCount(); ++i) {
                View child = mContainer.getChildAt(i);
                if (child instanceof ChromeImageButton button
                        && button.getId() != R.id.back_button) {
                    if (button.getDrawable() != null) {
                        iconWidthPx = button.getDrawable().getIntrinsicWidth();
                    }
                    break;
                }
            }
            searchButtonOffsetPx = (minTouchTargetPx - iconWidthPx) / 2;
        }

        View titleScrollView = (View) mContainer.getParent();
        if (titleScrollView == null) return;
        var params = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        if (params == null) return;
        params.setMarginStart(startMargin + offsetX - backButtonOffsetPx);
        params.setMarginEnd(startMargin + offsetX - searchButtonOffsetPx);
        titleScrollView.setLayoutParams(params);
    }

    private int getDimenPx(int res) {
        return mContext.getResources().getDimensionPixelSize(res);
    }

    public void onSaveInstanceState(Bundle outState) {
        outState.putInt(KEY_FIRST_VISIBLE_INDEX, mFirstVisibleTitleIndex);
        if (mCachedDeepLinkPath != null) {
            outState.putParcelableArrayList(
                    KEY_CACHED_DEEP_LINK_PATH, new ArrayList<>(mCachedDeepLinkPath));
        }
    }

    @Nullable List<SettingsIndexData.Entry> getInitialBreadcrumbPathForTesting() {
        return mInitialBreadcrumbPath;
    }
}
