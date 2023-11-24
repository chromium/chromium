// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * The view for the entire search resumption layout, including a header, an option button to
 * collapse or expand the suggestion section, and the section of a set of search suggestions.
 */
public class SearchResumptionModuleView extends LinearLayout {
    private View mHeaderView;
    private ImageView mOptionView;
    private SearchResumptionTileContainerView mTileContainerView;

    public SearchResumptionModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHeaderView = findViewById(R.id.search_resumption_module_header);
        mTileContainerView = findViewById(R.id.search_resumption_module_tiles_container);
        mOptionView = findViewById(R.id.header_option);
        configureExpandedCollapsed(
                !ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP,
                                false)
                /* shouldExpand= */ ,
                /* isFirstSetup= */ true);
    }

    void setExpandCollapseCallback(Callback<Boolean> callback) {
        mHeaderView.setOnClickListener(
                v -> {
                    boolean shouldExpand = !mTileContainerView.isExpanded();
                    configureExpandedCollapsed(shouldExpand, /* isFirstSetup= */ false);
                    callback.onResult(shouldExpand);
                });
    }

    void destroy() {
        mTileContainerView.destroy();
    }

    /** Configures expanding or collapsing the suggest sections. */
    private void configureExpandedCollapsed(boolean shouldExpand, boolean isFirstSetup) {
        if (isFirstSetup || mTileContainerView.isExpanded() != shouldExpand) {
            if (shouldExpand) {
                mOptionView.setImageResource(R.drawable.ic_expand_less_black_24dp);
            } else {
                mOptionView.setImageResource(R.drawable.ic_expand_more_black_24dp);
            }
            String collapseOrExpandedText =
                    getContext()
                            .getResources()
                            .getString(
                                    shouldExpand
                                            ? R.string.accessibility_expanded
                                            : R.string.accessibility_collapsed);
            String description =
                    getContext()
                            .getResources()
                            .getString(R.string.search_resumption_module_subtitle);
            mHeaderView.setContentDescription(description + " " + collapseOrExpandedText);
        }

        if (mTileContainerView.isExpanded() == shouldExpand) return;

        mTileContainerView.configureExpandedCollapsed(
                shouldExpand, /* isAnimationEnabled= */ !isFirstSetup);
    }
}
