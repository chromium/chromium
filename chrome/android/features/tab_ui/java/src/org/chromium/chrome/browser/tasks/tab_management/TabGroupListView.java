// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;

/** Conditionally displays empty state for the tab group pane. */
public class TabGroupListView extends FrameLayout {
    private RecyclerView mRecyclerView;
    private View mEmptyStateContainer;
    private TextView mEmptyStateSubheading;
    private UiConfig mUiConfig;

    /** Constructor for inflation. */
    public TabGroupListView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        Context context = getContext();
        mRecyclerView = findViewById(R.id.tab_group_list_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(context));

        mEmptyStateContainer = findViewById(R.id.empty_state_container);

        ImageView emptyStateIllustration = findViewById(R.id.empty_state_icon);
        Drawable illustration =
                AppCompatResources.getDrawable(
                        context, R.drawable.tab_group_list_empty_state_illustration);
        emptyStateIllustration.setImageDrawable(illustration);

        TextView emptyStateHeading = findViewById(R.id.empty_state_text_title);
        emptyStateHeading.setText(R.string.tab_groups_empty_header);
        mEmptyStateSubheading = findViewById(R.id.empty_state_text_description);

        mUiConfig = new UiConfig(this);
        mUiConfig.addObserver(this::onDisplayStyleChanged);
    }

    void setRecyclerViewAdapter(RecyclerView.Adapter adapter) {
        mRecyclerView.setAdapter(adapter);
    }

    void setEmptyStateVisible(boolean visible) {
        mEmptyStateContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        mRecyclerView.setVisibility(visible ? View.INVISIBLE : View.VISIBLE);
    }

    void setSyncEnabled(boolean enabled) {
        mEmptyStateSubheading.setText(
                enabled
                        ? R.string.tab_groups_empty_state_description
                        : R.string.tab_groups_empty_state_description_no_sync);
    }

    private void onDisplayStyleChanged(UiConfig.DisplayStyle newDisplayStyle) {
        int padding =
                SelectableListLayout.getPaddingForDisplayStyle(newDisplayStyle, getResources());
        mRecyclerView.setPaddingRelative(
                padding, mRecyclerView.getPaddingTop(), padding, mRecyclerView.getPaddingBottom());
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        if (mUiConfig != null) mUiConfig.updateDisplayStyle();
    }
}
