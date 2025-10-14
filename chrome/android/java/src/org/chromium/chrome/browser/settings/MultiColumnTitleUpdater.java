// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

/**
 * Observes MultiColumnSettings events, and updates the SettingsActivity's title and its detailed
 * pane titles.
 */
@NullMarked
class MultiColumnTitleUpdater implements MultiColumnSettings.Observer {

    /** Displays one component of the detailed pane fragment stack. */
    private static class DetailedTitle extends TextView {
        private final Callback<String> mSetter =
                (title) -> {
                    if (title == null) {
                        title = "";
                    }
                    setText(title);
                };

        private @Nullable ObservableSupplier<String> mSupplier;

        DetailedTitle(Context context) {
            super(context);

            // Use the same TextAppearance with the main settings title.
            setTextAppearance(R.style.TextAppearance_Headline_Primary);
        }

        void setSupplier(@Nullable ObservableSupplier<String> supplier) {
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

    /**
     * Keeps tracking the current main page title supplier. Null if not tracking, e.g. in two pane
     * mode.
     */
    private @Nullable ObservableSupplier<String> mCurrentPageTitle;

    MultiColumnTitleUpdater(
            MultiColumnSettings multiColumnSettings,
            Context context,
            LinearLayout container,
            Callback<String> mainTitleSetter) {
        mMultiColumnSettings = multiColumnSettings;
        mContext = context;
        mContainer = container;
        mMainTitleSetter = mainTitleSetter;
    }

    @Override
    public void onTitleUpdated() {
        updateMainTitle();
        updateDetailedPageTitle();
    }

    private void updateMainTitle() {
        // Unset if needed, first.
        if (mCurrentPageTitle != null) {
            mCurrentPageTitle.removeObserver(mMainTitleSetter);
        }

        var titles = mMultiColumnSettings.getTitles();
        if (mMultiColumnSettings.isTwoPane() || titles.isEmpty()) {
            // In the two pane mode, the main title is always "Settings".
            mMainTitleSetter.onResult(mContext.getString(R.string.settings));
            mCurrentPageTitle = null;
        } else {
            // Otherwise, use the last fragment, because it is what a user is seeing.
            mCurrentPageTitle = titles.get(titles.size() - 1).titleSupplier;
            mCurrentPageTitle.addSyncObserverAndCallIfNonNull(mMainTitleSetter);
        }
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

        // Padding for the chevron separator.
        int paddingPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_detailed_title_padding);

        var titles = mMultiColumnSettings.getTitles();
        for (int i = 0; i < titles.size(); ++i) {
            if (i != 0) {
                // '>' separator.
                var view = new ImageView(mContext);
                view.setPadding(paddingPx, 0, paddingPx, 0);
                view.setImageResource(R.drawable.chevron_right);
                mContainer.addView(view);
            }
            var view = new DetailedTitle(mContext);
            var title = titles.get(i);
            view.setSupplier(title.titleSupplier);

            final int backStackCount = title.backStackCount;
            view.setOnClickListener(
                    (View v) -> {
                        assert mMultiColumnSettings != null;
                        // Note: The current getBackStackEntryCount and recorded backStackCount
                        // can be same, e.g., if the user tabs the last component of the
                        // detailed title.
                        if (mMultiColumnSettings.getChildFragmentManager().getBackStackEntryCount()
                                > backStackCount) {
                            var entry =
                                    mMultiColumnSettings
                                            .getChildFragmentManager()
                                            .getBackStackEntryAt(backStackCount);
                            mMultiColumnSettings
                                    .getChildFragmentManager()
                                    .popBackStack(
                                            entry.getId(),
                                            FragmentManager.POP_BACK_STACK_INCLUSIVE);
                        }
                    });
            mContainer.addView(view);
        }
    }

    @Override
    public void onHeaderLayoutUpdated() {
        updateMainTitle();

        if (!mMultiColumnSettings.isTwoPane()) {
            // In the single pane mode, do not show the detailed title.
            mContainer.setVisibility(View.GONE);
            return;
        }

        // Enable detailed page title.
        mContainer.setVisibility(View.VISIBLE);

        // Set left margin to align with the detailed pane.
        View view = mMultiColumnSettings.getHeaderView();
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mContainer.getLayoutParams();
        params.setMargins(view.getLayoutParams().width, 0, 0, 0);
        mContainer.setLayoutParams(params);
        mContainer.invalidate();
    }
}
