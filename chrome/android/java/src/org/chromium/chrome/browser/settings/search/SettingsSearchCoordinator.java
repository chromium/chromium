// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Configuration;
import android.os.Handler;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.FragmentManager;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.MultiColumnSettings;

import java.util.function.BooleanSupplier;

/** The coordinator of search in Settings. TODO(jinsukkim): Build a proper MVC structure. */
@NullMarked
public class SettingsSearchCoordinator {
    private final AppCompatActivity mActivity;
    private final BooleanSupplier mUseMultiColumnSupplier;
    private @Nullable final MultiColumnSettings mMultiColumnSettings;

    // True if multiple-column Fragment is activated. Both the window width and the feature flag
    // condition should be met.
    private boolean mUseMultiColumn;

    /**
     * @param activity {@link SettingsActivity} object
     * @param useMultiColumnSupplier Supplier telling us whether the multi-column mode is on
     * @param multiColumnSettings {@link MultiColumnSettings} Fragment. Can be {@code null} unless
     *     the multi-column settings feature is enabled.
     */
    public SettingsSearchCoordinator(
            AppCompatActivity activity,
            BooleanSupplier useMultiColumnSupplier,
            @Nullable MultiColumnSettings multiColumnSettings) {
        mActivity = activity;
        mUseMultiColumnSupplier = useMultiColumnSupplier;
        mMultiColumnSettings = multiColumnSettings;
    }

    /** Initializes search UI, sets up listeners, backpress action handler, etc. */
    public void initializeSearchUi() {
        mUseMultiColumn = mUseMultiColumnSupplier.getAsBoolean();
        Toolbar actionBar = mActivity.findViewById(R.id.action_bar);
        ViewGroup appBar = mActivity.findViewById(R.id.app_bar_layout);
        ViewGroup searchBoxParent = mUseMultiColumn ? actionBar : appBar;
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_box, searchBoxParent, true);
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_query, actionBar, true);
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        if (mUseMultiColumn) {
            // Adjust the view width after the Fragment layout is completed.
            new Handler().post(this::updateDetailPanelWidth);
        }
        searchBox.setOnClickListener(
                v -> {
                    searchBox.setVisibility(View.GONE);
                    queryContainer.setVisibility(View.VISIBLE);
                    if (!mUseMultiColumn) {
                        assumeNonNull(mActivity.getSupportActionBar())
                                .setDisplayHomeAsUpEnabled(false);
                    }
                    // TODO(jinsukkim): Initialize search query widget.
                });
        View backToSettings = mActivity.findViewById(R.id.back_arrow_icon);
        backToSettings.setOnClickListener(
                v -> {
                    queryContainer.setVisibility(View.GONE);
                    searchBox.setVisibility(View.VISIBLE);
                    if (!mUseMultiColumn) {
                        assumeNonNull(mActivity.getSupportActionBar())
                                .setDisplayHomeAsUpEnabled(true);
                    }
                    getSettingsFragmentManager().popBackStack();
                    // TODO(jinsukkim): Complete back action.
                });
        actionBar.setOverflowIcon(null);
    }

    private FragmentManager getSettingsFragmentManager() {
        if (mUseMultiColumn) {
            return assumeNonNull(mMultiColumnSettings).getChildFragmentManager();
        } else {
            return mActivity.getSupportFragmentManager();
        }
    }

    private void updateDetailPanelWidth() {
        assert mUseMultiColumn : "Should be called in multi-column mode only.";

        var windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(mActivity);
        int endPaddingPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_detail_panel_end_padding);
        int headerWidthPx = assumeNonNull(mMultiColumnSettings).getHeaderPanelWidthPx();
        int detailViewWidthPx = windowMetrics.getBounds().width() - headerWidthPx - endPaddingPx;
        View searchBox = mActivity.findViewById(R.id.search_box);
        var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
        lp.width = detailViewWidthPx;
        lp.gravity = Gravity.END;
        searchBox.setLayoutParams(lp);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        LayoutParams qlp = queryContainer.getLayoutParams();
        qlp.width = detailViewWidthPx;
        queryContainer.setLayoutParams(qlp);
    }

    /**
     * Updates the UI layout for the changes in column mode/window width.
     *
     * @see {@link Activity#onConfigurationChanged(Configuration)}.
     */
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        boolean useMultiColumn = mUseMultiColumnSupplier.getAsBoolean();

        if (useMultiColumn == mUseMultiColumn) {
            if (mUseMultiColumn) new Handler().post(this::updateDetailPanelWidth);
            return;
        }

        mUseMultiColumn = mUseMultiColumnSupplier.getAsBoolean();
        View searchBox = mActivity.findViewById(R.id.search_box);
        ViewGroup searchBoxParent =
                mActivity.findViewById(useMultiColumn ? R.id.app_bar_layout : R.id.action_bar);
        searchBoxParent.removeView(searchBox);
        new Handler().post(() -> switchSearchUiLayout(searchBox));
    }

    private void switchSearchUiLayout(View searchBox) {
        if (mUseMultiColumn) {
            ViewGroup actionBar = mActivity.findViewById(R.id.action_bar);
            actionBar.addView(searchBox);
            updateDetailPanelWidth();
        } else {
            ViewGroup appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
            appBarLayout.addView(searchBox);
            View queryContainer = mActivity.findViewById(R.id.search_query_container);
            LayoutParams lp = searchBox.getLayoutParams();
            lp.width = LayoutParams.MATCH_PARENT;
            searchBox.setLayoutParams(lp);
            lp = queryContainer.getLayoutParams();
            lp.width = LayoutParams.MATCH_PARENT;
            queryContainer.setLayoutParams(lp);
        }
    }
}
