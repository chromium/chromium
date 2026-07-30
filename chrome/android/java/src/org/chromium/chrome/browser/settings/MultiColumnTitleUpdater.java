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
import android.view.View;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.appcompat.widget.AppCompatTextView;
import androidx.appcompat.widget.TooltipCompat;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
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

    MultiColumnTitleUpdater(
            @Nullable Bundle savedInstanceState,
            MultiColumnSettings multiColumnSettings,
            Context context,
            LinearLayout container,
            Callback<String> mainTitleSetter,
            Callback<@Nullable String> titleTapCallback,
            @Nullable List<SettingsIndexData.Entry> initialBreadcrumbPath) {
        mMultiColumnSettings = multiColumnSettings;
        mContext = context;
        mContainer = container;
        mMainTitleSetter = mainTitleSetter;
        mTitleTapCallback = titleTapCallback;
        mInitialBreadcrumbPath = initialBreadcrumbPath;

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
                    } else {
                        // note: we cannot traverse in the reverse order here unlike above,
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

        List<MultiColumnSettings.Title> titles = initTitlesList();

        // Padding for the chevron separator.
        int paddingPx = getDimenPx(R.dimen.settings_detailed_title_padding);

        float scaleX = LocalizationUtils.isLayoutRtl() ? -1f : 1f;

        if (SettingsInTab.isEnabled() && titles.size() > 1) {
            // Set up a back button to go to the section for the previous title.
            int prevIndex = titles.size() - 2;
            var prevTitle = titles.get(prevIndex);
            var backButton = new ChromeImageButton(mContext);
            backButton.setImageResource(R.drawable.ic_arrow_back_24dp);
            // Ensure icon isn't stretched by the larger touch target.
            backButton.setScaleType(ImageView.ScaleType.CENTER);
            // Provide material design circular hover highlight and ripple.
            backButton.setBackgroundResource(R.drawable.default_icon_background);
            // Ensure size is large enough for touch accessibility.
            int minTouchTargetPx = getDimenPx(R.dimen.min_touch_target_size);
            backButton.setMinimumWidth(minTouchTargetPx);
            backButton.setMinimumHeight(minTouchTargetPx);
            // Offset the button to the left so it aligns with the left edge of the cards below.
            var layoutParams = new LinearLayout.LayoutParams(LAYOUT_CENTER_VERTICAL);
            assertNonNull(backButton.getDrawable());
            int iconWidthPx = backButton.getDrawable().getIntrinsicWidth();
            layoutParams.setMarginStart(-(minTouchTargetPx - iconWidthPx) / 2);
            backButton.setLayoutParams(layoutParams);
            backButton.setOnClickListener(v -> navigateToTitle(prevTitle, prevIndex));
            // Set both accessibility content description and tooltip.
            TooltipCompat.setTooltipText(backButton, mContext.getString(R.string.back));
            mContainer.addView(backButton);
        }

        for (int i = 0; i < titles.size(); ++i) {
            if (i < mFirstVisibleTitleIndex) continue;

            if (i != mFirstVisibleTitleIndex) {
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
        }

        // Make the last-added/tapped one visible after adding titles.
        if (mContainer.getParent() instanceof HorizontalScrollView scrollView) {
            scrollView.post(() -> scrollView.fullScroll(HorizontalScrollView.FOCUS_RIGHT));
        }
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

        maybeUpdateStartMargin();
    }

    @Override
    public void onDetailLayoutUpdated() {
        maybeUpdateStartMargin();
    }

    private void maybeUpdateStartMargin() {
        View detailView = mMultiColumnSettings.getDetailView();
        View recyclerView = detailView.findViewById(R.id.recycler_view);
        if (recyclerView == null) return;

        int widthPx = recyclerView.getWidth();
        if (widthPx == 0) return;

        int maxDetailWidthPx = getDimenPx(R.dimen.settings_min_multi_column_screen_width);
        int minPaddingPx = getDimenPx(R.dimen.settings_multi_column_pane_gap);
        int startMargin = getDimenPx(R.dimen.settings_detailed_title_start_margin);
        int excessPx = widthPx - maxDetailWidthPx - minPaddingPx * 2;
        int offsetX = minPaddingPx + (excessPx > 0 ? excessPx / 2 : 0);
        View titleScrollView = (View) mContainer.getParent();
        var params = (RelativeLayout.LayoutParams) titleScrollView.getLayoutParams();
        params.setMarginStart(startMargin + offsetX);
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
